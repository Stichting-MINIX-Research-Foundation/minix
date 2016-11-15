/*
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Reduces the resources demanded by TCP sessions in TIME_WAIT-state using
 * methods called Vestigial Time-Wait (VTW) and Maximum Segment Lifetime
 * Truncation (MSLT).
 * 
 * MSLT and VTW were contributed by Coyote Point Systems, Inc.
 * 
 * Even after a TCP session enters the TIME_WAIT state, its corresponding
 * socket and protocol control blocks (PCBs) stick around until the TCP
 * Maximum Segment Lifetime (MSL) expires.  On a host whose workload
 * necessarily creates and closes down many TCP sockets, the sockets & PCBs
 * for TCP sessions in TIME_WAIT state amount to many megabytes of dead
 * weight in RAM.
 * 
 * Maximum Segment Lifetimes Truncation (MSLT) assigns each TCP session to
 * a class based on the nearness of the peer.  Corresponding to each class
 * is an MSL, and a session uses the MSL of its class.  The classes are
 * loopback (local host equals remote host), local (local host and remote
 * host are on the same link/subnet), and remote (local host and remote
 * host communicate via one or more gateways).  Classes corresponding to
 * nearer peers have lower MSLs by default: 2 seconds for loopback, 10
 * seconds for local, 60 seconds for remote.  Loopback and local sessions
 * expire more quickly when MSLT is used.
 * 
 * Vestigial Time-Wait (VTW) replaces a TIME_WAIT session's PCB/socket
 * dead weight with a compact representation of the session, called a
 * "vestigial PCB".  VTW data structures are designed to be very fast and
 * memory-efficient: for fast insertion and lookup of vestigial PCBs,
 * the PCBs are stored in a hash table that is designed to minimize the
 * number of cacheline visits per lookup/insertion.  The memory both
 * for vestigial PCBs and for elements of the PCB hashtable come from
 * fixed-size pools, and linked data structures exploit this to conserve
 * memory by representing references with a narrow index/offset from the
 * start of a pool instead of a pointer.  When space for new vestigial PCBs
 * runs out, VTW makes room by discarding old vestigial PCBs, oldest first.
 * VTW cooperates with MSLT.
 * 
 * It may help to think of VTW as a "FIN cache" by analogy to the SYN
 * cache.
 * 
 * A 2.8-GHz Pentium 4 running a test workload that creates TIME_WAIT
 * sessions as fast as it can is approximately 17% idle when VTW is active
 * versus 0% idle when VTW is inactive.  It has 103 megabytes more free RAM
 * when VTW is active (approximately 64k vestigial PCBs are created) than
 * when it is inactive.
 */

#include <sys/cdefs.h>

#ifdef _KERNEL_OPT
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_inet_csum.h"
#include "opt_tcp_debug.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/pool.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/in_offload.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_private.h>
#include <netinet/tcpip.h>

#include <netinet/tcp_vtw.h>

__KERNEL_RCSID(0, "$NetBSD: tcp_vtw.c,v 1.14 2015/08/24 22:21:26 pooka Exp $");

#define db_trace(__a, __b)	do { } while (/*CONSTCOND*/0)

static void vtw_debug_init(void);

fatp_ctl_t fat_tcpv4;
fatp_ctl_t fat_tcpv6;
vtw_ctl_t  vtw_tcpv4[VTW_NCLASS];
vtw_ctl_t  vtw_tcpv6[VTW_NCLASS];
vtw_stats_t vtw_stats;

/* We provide state for the lookup_ports iterator.
 * As currently we are netlock-protected, there is one.
 * If we were finer-grain, we would have one per CPU.
 * I do not want to be in the business of alloc/free.
 * The best alternate would be allocate on the caller's
 * stack, but that would require them to know the struct,
 * or at least the size.
 * See how she goes.
 */
struct tcp_ports_iterator {
	union {
		struct in_addr	v4;
		struct in6_addr	v6;
	}		addr;
	u_int		port;

	uint32_t	wild	: 1;

	vtw_ctl_t	*ctl;
	fatp_t		*fp;

	uint16_t	slot_idx;
	uint16_t	ctl_idx;
};

static struct tcp_ports_iterator tcp_ports_iterator_v4;
static struct tcp_ports_iterator tcp_ports_iterator_v6;

static int vtw_age(vtw_ctl_t *, struct timeval *);

/*!\brief allocate a fat pointer from a collection.
 */
static fatp_t *
fatp_alloc(fatp_ctl_t *fat)
{
	fatp_t	*fp	= 0;

	if (fat->nfree) {
		fp = fat->free;
		if (fp) {
			fat->free = fatp_next(fat, fp);
			--fat->nfree;
			++fat->nalloc;
			fp->nxt = 0;

			KASSERT(!fp->inuse);
		}
	}

	return fp;
}

/*!\brief free a fat pointer.
 */
static void
fatp_free(fatp_ctl_t *fat, fatp_t *fp)
{
	if (fp) {
		KASSERT(!fp->inuse);
		KASSERT(!fp->nxt);

		fp->nxt = fatp_index(fat, fat->free);
		fat->free = fp;

		++fat->nfree;
		--fat->nalloc;
	}
}

/*!\brief initialise a collection of fat pointers.
 *
 *\param n	# hash buckets
 *\param m	total # fat pointers to allocate
 *
 * We allocate 2x as much, as we have two hashes: full and lport only.
 */
static void
fatp_init(fatp_ctl_t *fat, uint32_t n, uint32_t m,
    fatp_t *fat_base, fatp_t **fat_hash)
{
	fatp_t	*fp;

	KASSERT(n <= FATP_MAX / 2);

	fat->hash = fat_hash;
	fat->base = fat_base;

	fat->port = &fat->hash[m];

	fat->mask   = m - 1;	// ASSERT is power of 2 (m)
	fat->lim    = fat->base + 2*n - 1;
	fat->nfree  = 0;
	fat->nalloc = 2*n;

	/* Initialise the free list.
	 */
	for (fp = fat->lim; fp >= fat->base; --fp) {
		fatp_free(fat, fp);
	}
}

/*
 * The `xtra' is XORed into the tag stored.
 */
static uint32_t fatp_xtra[] = {
	0x11111111,0x22222222,0x33333333,0x44444444,
	0x55555555,0x66666666,0x77777777,0x88888888,
	0x12121212,0x21212121,0x34343434,0x43434343,
	0x56565656,0x65656565,0x78787878,0x87878787,
	0x11221122,0x22112211,0x33443344,0x44334433,
	0x55665566,0x66556655,0x77887788,0x88778877,
	0x11112222,0x22221111,0x33334444,0x44443333,
	0x55556666,0x66665555,0x77778888,0x88887777,
};

/*!\brief turn a {fatp_t*,slot} into an integral key.
 *
 * The key can be used to obtain the fatp_t, and the slot,
 * as it directly encodes them.
 */
static inline uint32_t
fatp_key(fatp_ctl_t *fat, fatp_t *fp, uint32_t slot)
{
	CTASSERT(CACHE_LINE_SIZE == 32 ||
	         CACHE_LINE_SIZE == 64 ||
		 CACHE_LINE_SIZE == 128);

	switch (fatp_ntags()) {
	case 7:
		return (fatp_index(fat, fp) << 3) | slot;
	case 15:
		return (fatp_index(fat, fp) << 4) | slot;
	case 31:
		return (fatp_index(fat, fp) << 5) | slot;
	default:
		KASSERT(0 && "no support, for no good reason");
		return ~0;
	}
}

static inline uint32_t
fatp_slot_from_key(fatp_ctl_t *fat, uint32_t key)
{
	CTASSERT(CACHE_LINE_SIZE == 32 ||
	         CACHE_LINE_SIZE == 64 ||
		 CACHE_LINE_SIZE == 128);

	switch (fatp_ntags()) {
	case 7:
		return key & 7;
	case 15:
		return key & 15;
	case 31:
		return key & 31;
	default:
		KASSERT(0 && "no support, for no good reason");
		return ~0;
	}
}

static inline fatp_t *
fatp_from_key(fatp_ctl_t *fat, uint32_t key)
{
	CTASSERT(CACHE_LINE_SIZE == 32 ||
	         CACHE_LINE_SIZE == 64 ||
		 CACHE_LINE_SIZE == 128);

	switch (fatp_ntags()) {
	case 7:
		key >>= 3;
		break;
	case 15:
		key >>= 4;
		break;
	case 31:
		key >>= 5;
		break;
	default:
		KASSERT(0 && "no support, for no good reason");
		return 0;
	}

	return key ? fat->base + key - 1 : 0;
}

static inline uint32_t
idx_encode(vtw_ctl_t *ctl, uint32_t idx)
{
	return (idx << ctl->idx_bits) | idx;
}

static inline uint32_t
idx_decode(vtw_ctl_t *ctl, uint32_t bits)
{
	uint32_t	idx	= bits & ctl->idx_mask;

	if (idx_encode(ctl, idx) == bits)
		return idx;
	else
		return ~0;
}

/*!\brief	insert index into fatp hash
 *
 *\param	idx	-	index of element being placed in hash chain
 *\param	tag	-	32-bit tag identifier
 *
 *\returns
 *	value which can be used to locate entry.
 *
 *\note
 *	we rely on the fact that there are unused high bits in the index
 *	for verification purposes on lookup.
 */

static inline uint32_t
fatp_vtw_inshash(fatp_ctl_t *fat, uint32_t idx, uint32_t tag, int which,
    void *dbg)
{
	fatp_t	*fp;
	fatp_t	**hash = (which ? fat->port : fat->hash);
	int	i;

	fp = hash[tag & fat->mask];

	while (!fp || fatp_full(fp)) {
		fatp_t	*fq;

		/* All entries are inuse at the top level.
		 * We allocate a spare, and push the top level
		 * down one.  All entries in the fp we push down
		 * (think of a tape worm here) will be expelled sooner than
		 * any entries added subsequently to this hash bucket.
		 * This is a property of the time waits we are exploiting.
		 */

		fq = fatp_alloc(fat);
		if (!fq) {
			vtw_age(fat->vtw, 0);
			fp = hash[tag & fat->mask];
			continue;
		}

		fq->inuse = 0;
		fq->nxt   = fatp_index(fat, fp);

		hash[tag & fat->mask] = fq;

		fp = fq;
	}

	KASSERT(!fatp_full(fp));

	/* Fill highest index first.  Lookup is lowest first.
	 */
	for (i = fatp_ntags(); --i >= 0; ) {
		if (!((1 << i) & fp->inuse)) {
			break;
		}
	}

	fp->inuse |= 1 << i;
	fp->tag[i] = tag ^ idx_encode(fat->vtw, idx) ^ fatp_xtra[i];

	db_trace(KTR_VTW
		 , (fp, "fat: inuse %5.5x tag[%x] %8.8x"
		    , fp->inuse
		    , i, fp->tag[i]));

	return fatp_key(fat, fp, i);
}

static inline int
vtw_alive(const vtw_t *vtw)
{
	return vtw->hashed && vtw->expire.tv_sec;
}

static inline uint32_t
vtw_index_v4(vtw_ctl_t *ctl, vtw_v4_t *v4)
{
	if (ctl->base.v4 <= v4 && v4 <= ctl->lim.v4)
		return v4 - ctl->base.v4;

	KASSERT(0 && "vtw out of bounds");

	return ~0;
}

static inline uint32_t
vtw_index_v6(vtw_ctl_t *ctl, vtw_v6_t *v6)
{
	if (ctl->base.v6 <= v6 && v6 <= ctl->lim.v6)
		return v6 - ctl->base.v6;

	KASSERT(0 && "vtw out of bounds");

	return ~0;
}

static inline uint32_t
vtw_index(vtw_ctl_t *ctl, vtw_t *vtw)
{
	if (ctl->clidx)
		ctl = ctl->ctl;

	if (ctl->is_v4)
		return vtw_index_v4(ctl, (vtw_v4_t *)vtw);

	if (ctl->is_v6)
		return vtw_index_v6(ctl, (vtw_v6_t *)vtw);

	KASSERT(0 && "neither 4 nor 6.  most curious.");

	return ~0;
}

static inline vtw_t *
vtw_from_index(vtw_ctl_t *ctl, uint32_t idx)
{
	if (ctl->clidx)
		ctl = ctl->ctl;

	/* See if the index looks like it might be an index.
	 * Bits on outside of the valid index bits is a give away.
	 */
	idx = idx_decode(ctl, idx);

	if (idx == ~0) {
		return 0;
	} else if (ctl->is_v4) {
		vtw_v4_t	*vtw = ctl->base.v4 + idx;

		return (ctl->base.v4 <= vtw && vtw <= ctl->lim.v4)
			? &vtw->common : 0;
	} else if (ctl->is_v6) {
		vtw_v6_t	*vtw = ctl->base.v6 + idx;

		return (ctl->base.v6 <= vtw && vtw <= ctl->lim.v6)
			? &vtw->common : 0;
	} else {
		KASSERT(0 && "badness");
		return 0;
	}
}

/*!\brief return the next vtw after this one.
 *
 * Due to the differing sizes of the entries in differing
 * arenas, we have to ensure we ++ the correct pointer type.
 *
 * Also handles wrap.
 */
static inline vtw_t *
vtw_next(vtw_ctl_t *ctl, vtw_t *vtw)
{
	if (ctl->is_v4) {
		vtw_v4_t	*v4 = (void*)vtw;

		vtw = &(++v4)->common;
	} else {
		vtw_v6_t	*v6 = (void*)vtw;

		vtw = &(++v6)->common;
	}

	if (vtw > ctl->lim.v)
		vtw = ctl->base.v;

	return vtw;
}

/*!\brief	remove entry from FATP hash chains
 */
static inline void
vtw_unhash(vtw_ctl_t *ctl, vtw_t *vtw)
{
	fatp_ctl_t	*fat	= ctl->fat;
	fatp_t		*fp;
	uint32_t	key = vtw->key;
	uint32_t	tag, slot, idx;
	vtw_v4_t	*v4 = (void*)vtw;
	vtw_v6_t	*v6 = (void*)vtw;

	if (!vtw->hashed) {
		KASSERT(0 && "unhashed");
		return;
	}

	if (fat->vtw->is_v4) {
		tag = v4_tag(v4->faddr, v4->fport, v4->laddr, v4->lport);
	} else if (fat->vtw->is_v6) {
		tag = v6_tag(&v6->faddr, v6->fport, &v6->laddr, v6->lport);
	} else {
		tag = 0;
		KASSERT(0 && "not reached");
	}

	/* Remove from fat->hash[]
	 */
	slot = fatp_slot_from_key(fat, key);
	fp   = fatp_from_key(fat, key);
	idx  = vtw_index(ctl, vtw);

	db_trace(KTR_VTW
		 , (fp, "fat: del inuse %5.5x slot %x idx %x key %x tag %x"
		    , fp->inuse, slot, idx, key, tag));

	KASSERT(fp->inuse & (1 << slot));
	KASSERT(fp->tag[slot] == (tag ^ idx_encode(ctl, idx)
				  ^ fatp_xtra[slot]));

	if ((fp->inuse & (1 << slot))
	    && fp->tag[slot] == (tag ^ idx_encode(ctl, idx)
				 ^ fatp_xtra[slot])) {
		fp->inuse ^= 1 << slot;
		fp->tag[slot] = 0;

		/* When we delete entries, we do not compact.  This is
		 * due to temporality.  We add entries, and they
		 * (eventually) expire. Older entries will be further
		 * down the chain.
		 */
		if (!fp->inuse) {
			uint32_t hi = tag & fat->mask;
			fatp_t	*fq = 0;
			fatp_t	*fr = fat->hash[hi];

			while (fr && fr != fp) {
				fr = fatp_next(fat, fq = fr);
			}

			if (fr == fp) {
				if (fq) {
					fq->nxt = fp->nxt;
					fp->nxt = 0;
					fatp_free(fat, fp);
				} else {
					KASSERT(fat->hash[hi] == fp);

					if (fp->nxt) {
						fat->hash[hi]
							= fatp_next(fat, fp);
						fp->nxt = 0;
						fatp_free(fat, fp);
					} else {
						/* retain for next use.
						 */
						;
					}
				}
			} else {
				fr = fat->hash[hi];

				do {
					db_trace(KTR_VTW
						 , (fr
						    , "fat:*del inuse %5.5x"
						    " nxt %x"
						    , fr->inuse, fr->nxt));

					fr = fatp_next(fat, fq = fr);
				} while (fr && fr != fp);

				KASSERT(0 && "oops");
			}
		}
		vtw->key ^= ~0;
	}
	
	if (fat->vtw->is_v4) {
		tag = v4_port_tag(v4->lport);
	} else if (fat->vtw->is_v6) {
		tag = v6_port_tag(v6->lport);
	}

	/* Remove from fat->port[]
	 */
	key  = vtw->port_key;
	slot = fatp_slot_from_key(fat, key);
	fp   = fatp_from_key(fat, key);
	idx  = vtw_index(ctl, vtw);

	db_trace(KTR_VTW
		 , (fp, "fatport: del inuse %5.5x"
		    " slot %x idx %x key %x tag %x"
		    , fp->inuse, slot, idx, key, tag));

	KASSERT(fp->inuse & (1 << slot));
	KASSERT(fp->tag[slot] == (tag ^ idx_encode(ctl, idx)
				  ^ fatp_xtra[slot]));

	if ((fp->inuse & (1 << slot))
	    && fp->tag[slot] == (tag ^ idx_encode(ctl, idx)
				 ^ fatp_xtra[slot])) {
		fp->inuse ^= 1 << slot;
		fp->tag[slot] = 0;

		if (!fp->inuse) {
			uint32_t hi = tag & fat->mask;
			fatp_t	*fq = 0;
			fatp_t	*fr = fat->port[hi];

			while (fr && fr != fp) {
				fr = fatp_next(fat, fq = fr);
			}

			if (fr == fp) {
				if (fq) {
					fq->nxt = fp->nxt;
					fp->nxt = 0;
					fatp_free(fat, fp);
				} else {
					KASSERT(fat->port[hi] == fp);

					if (fp->nxt) {
						fat->port[hi]
							= fatp_next(fat, fp);
						fp->nxt = 0;
						fatp_free(fat, fp);
					} else {
						/* retain for next use.
						 */
						;
					}
				}
			}
		}
		vtw->port_key ^= ~0;
	}

	vtw->hashed = 0;
}

/*!\brief	remove entry from hash, possibly free.
 */
void
vtw_del(vtw_ctl_t *ctl, vtw_t *vtw)
{
	KASSERT(mutex_owned(softnet_lock));

	if (vtw->hashed) {
		++vtw_stats.del;
		vtw_unhash(ctl, vtw);
	}

	/* We only delete the oldest entry.
	 */
	if (vtw != ctl->oldest.v)
		return;

	--ctl->nalloc;
	++ctl->nfree;

	vtw->expire.tv_sec  = 0;
	vtw->expire.tv_usec = ~0;

	if (!ctl->nalloc)
		ctl->oldest.v = 0;

	ctl->oldest.v = vtw_next(ctl, vtw);
}

/*!\brief	insert vestigial timewait in hash chain
 */
static void
vtw_inshash_v4(vtw_ctl_t *ctl, vtw_t *vtw)
{
	uint32_t	idx	= vtw_index(ctl, vtw);
	uint32_t	tag;
	vtw_v4_t	*v4 = (void*)vtw;

	KASSERT(mutex_owned(softnet_lock));
	KASSERT(!vtw->hashed);
	KASSERT(ctl->clidx == vtw->msl_class);

	++vtw_stats.ins;

	tag = v4_tag(v4->faddr, v4->fport,
		     v4->laddr, v4->lport);

	vtw->key = fatp_vtw_inshash(ctl->fat, idx, tag, 0, vtw);

	db_trace(KTR_VTW, (ctl
			   , "vtw: ins %8.8x:%4.4x %8.8x:%4.4x"
			   " tag %8.8x key %8.8x"
			   , v4->faddr, v4->fport
			   , v4->laddr, v4->lport
			   , tag
			   , vtw->key));

	tag = v4_port_tag(v4->lport);
	vtw->port_key = fatp_vtw_inshash(ctl->fat, idx, tag, 1, vtw);

	db_trace(KTR_VTW, (ctl, "vtw: ins %P - %4.4x tag %8.8x key %8.8x"
			   , v4->lport, v4->lport
			   , tag
			   , vtw->key));

	vtw->hashed = 1;
}

/*!\brief	insert vestigial timewait in hash chain
 */
static void
vtw_inshash_v6(vtw_ctl_t *ctl, vtw_t *vtw)
{
	uint32_t	idx	= vtw_index(ctl, vtw);
	uint32_t	tag;
	vtw_v6_t	*v6	= (void*)vtw;

	KASSERT(mutex_owned(softnet_lock));
	KASSERT(!vtw->hashed);
	KASSERT(ctl->clidx == vtw->msl_class);

	++vtw_stats.ins;

	tag = v6_tag(&v6->faddr, v6->fport,
		     &v6->laddr, v6->lport);

	vtw->key = fatp_vtw_inshash(ctl->fat, idx, tag, 0, vtw);

	tag = v6_port_tag(v6->lport);
	vtw->port_key = fatp_vtw_inshash(ctl->fat, idx, tag, 1, vtw);

	db_trace(KTR_VTW, (ctl, "vtw: ins %P - %4.4x tag %8.8x key %8.8x"
			   , v6->lport, v6->lport
			   , tag
			   , vtw->key));

	vtw->hashed = 1;
}

static vtw_t *
vtw_lookup_hash_v4(vtw_ctl_t *ctl, uint32_t faddr, uint16_t fport
				 , uint32_t laddr, uint16_t lport
				 , int which)
{
	vtw_v4_t	*v4;
	vtw_t		*vtw;
	uint32_t	tag;
	fatp_t		*fp;
	int		i;
	uint32_t	fatps = 0, probes = 0, losings = 0;

	if (!ctl || !ctl->fat)
		return 0;

	++vtw_stats.look[which];

	if (which) {
		tag = v4_port_tag(lport);
		fp  = ctl->fat->port[tag & ctl->fat->mask];
	} else {
		tag = v4_tag(faddr, fport, laddr, lport);
		fp  = ctl->fat->hash[tag & ctl->fat->mask];
	}

	while (fp && fp->inuse) {
		uint32_t	inuse = fp->inuse;

		++fatps;

		for (i = 0; inuse && i < fatp_ntags(); ++i) {
			uint32_t	idx;

			if (!(inuse & (1 << i)))
				continue;

			inuse ^= 1 << i;

			++probes;
			++vtw_stats.probe[which];

			idx = fp->tag[i] ^ tag ^ fatp_xtra[i];
			vtw = vtw_from_index(ctl, idx);

			if (!vtw) {
				/* Hopefully fast path.
				 */
				db_trace(KTR_VTW
					 , (fp, "vtw: fast %A:%P %A:%P"
					    " idx %x tag %x"
					    , faddr, fport
					    , laddr, lport
					    , idx, tag));
				continue;
			}

			v4 = (void*)vtw;

			/* The de-referencing of vtw is what we want to avoid.
			 * Losing.
			 */
			if (vtw_alive(vtw)
			    && ((which ? vtw->port_key : vtw->key)
				== fatp_key(ctl->fat, fp, i))
			    && (which
				|| (v4->faddr == faddr && v4->laddr == laddr
				    && v4->fport == fport))
			    && v4->lport == lport) {
				++vtw_stats.hit[which];

				db_trace(KTR_VTW
					 , (fp, "vtw: hit %8.8x:%4.4x"
					    " %8.8x:%4.4x idx %x key %x"
					    , faddr, fport
					    , laddr, lport
					    , idx_decode(ctl, idx), vtw->key));

				KASSERT(vtw->hashed);

				goto out;
			}
			++vtw_stats.losing[which];
			++losings;
			
			if (vtw_alive(vtw)) {
				db_trace(KTR_VTW
					 , (fp, "vtw:!mis %8.8x:%4.4x"
					    " %8.8x:%4.4x key %x tag %x"
					    , faddr, fport
					    , laddr, lport
					    , fatp_key(ctl->fat, fp, i)
					    , v4_tag(faddr, fport
						     , laddr, lport)));
				db_trace(KTR_VTW
					 , (vtw, "vtw:!mis %8.8x:%4.4x"
					    " %8.8x:%4.4x key %x tag %x"
					    , v4->faddr, v4->fport
					    , v4->laddr, v4->lport
					    , vtw->key
					    , v4_tag(v4->faddr, v4->fport
						     , v4->laddr, v4->lport)));

				if (vtw->key == fatp_key(ctl->fat, fp, i)) {
					db_trace(KTR_VTW
						 , (vtw, "vtw:!mis %8.8x:%4.4x"
						    " %8.8x:%4.4x key %x"
						    " which %x"
						    , v4->faddr, v4->fport
						    , v4->laddr, v4->lport
						    , vtw->key
						    , which));

				} else {
					db_trace(KTR_VTW
						 , (vtw
						    , "vtw:!mis"
						    " key %8.8x != %8.8x"
						    " idx %x i %x which %x"
						    , vtw->key
						    , fatp_key(ctl->fat, fp, i)
						    , idx_decode(ctl, idx)
						    , i
						    , which));
				}
			} else {
				db_trace(KTR_VTW
					 , (fp
					    , "vtw:!mis free entry"
					    " idx %x vtw %p which %x"
					    , idx_decode(ctl, idx)
					    , vtw, which));
			}
		}

		if (fp->nxt) {
			fp = fatp_next(ctl->fat, fp);
		} else {
			break;
		}
	}
	++vtw_stats.miss[which];
	vtw = 0;
out:
	if (fatps > vtw_stats.max_chain[which])
		vtw_stats.max_chain[which] = fatps;
	if (probes > vtw_stats.max_probe[which])
		vtw_stats.max_probe[which] = probes;
	if (losings > vtw_stats.max_loss[which])
		vtw_stats.max_loss[which] = losings;

	return vtw;
}

static vtw_t *
vtw_lookup_hash_v6(vtw_ctl_t *ctl, const struct in6_addr *faddr, uint16_t fport
				 , const struct in6_addr *laddr, uint16_t lport
				 , int which)
{
	vtw_v6_t	*v6;
	vtw_t		*vtw;
	uint32_t	tag;
	fatp_t		*fp;
	int		i;
	uint32_t	fatps = 0, probes = 0, losings = 0;

	++vtw_stats.look[which];

	if (!ctl || !ctl->fat)
		return 0;

	if (which) {
		tag = v6_port_tag(lport);
		fp  = ctl->fat->port[tag & ctl->fat->mask];
	} else {
		tag = v6_tag(faddr, fport, laddr, lport);
		fp  = ctl->fat->hash[tag & ctl->fat->mask];
	}

	while (fp && fp->inuse) {
		uint32_t	inuse = fp->inuse;

		++fatps;

		for (i = 0; inuse && i < fatp_ntags(); ++i) {
			uint32_t	idx;

			if (!(inuse & (1 << i)))
				continue;

			inuse ^= 1 << i;

			++probes;
			++vtw_stats.probe[which];

			idx = fp->tag[i] ^ tag ^ fatp_xtra[i];
			vtw = vtw_from_index(ctl, idx);

			db_trace(KTR_VTW
				 , (fp, "probe: %2d %6A:%4.4x %6A:%4.4x idx %x"
				    , i
				    , db_store(faddr, sizeof (*faddr)), fport
				    , db_store(laddr, sizeof (*laddr)), lport
				    , idx_decode(ctl, idx)));

			if (!vtw) {
				/* Hopefully fast path.
				 */
				continue;
			}

			v6 = (void*)vtw;

			if (vtw_alive(vtw)
			    && ((which ? vtw->port_key : vtw->key)
				== fatp_key(ctl->fat, fp, i))
			    && v6->lport == lport
			    && (which
				|| (v6->fport == fport
				    && !bcmp(&v6->faddr, faddr, sizeof (*faddr))
				    && !bcmp(&v6->laddr, laddr
					     , sizeof (*laddr))))) {
				++vtw_stats.hit[which];

				KASSERT(vtw->hashed);
				goto out;
			} else {
				++vtw_stats.losing[which];
				++losings;
			}
		}

		if (fp->nxt) {
			fp = fatp_next(ctl->fat, fp);
		} else {
			break;
		}
	}
	++vtw_stats.miss[which];
	vtw = 0;
out:
	if (fatps > vtw_stats.max_chain[which])
		vtw_stats.max_chain[which] = fatps;
	if (probes > vtw_stats.max_probe[which])
		vtw_stats.max_probe[which] = probes;
	if (losings > vtw_stats.max_loss[which])
		vtw_stats.max_loss[which] = losings;

	return vtw;
}

/*!\brief port iterator
 */
static vtw_t *
vtw_next_port_v4(struct tcp_ports_iterator *it)
{
	vtw_ctl_t	*ctl = it->ctl;
	vtw_v4_t	*v4;
	vtw_t		*vtw;
	uint32_t	tag;
	uint16_t	lport = it->port;
	fatp_t		*fp;
	int		i;
	uint32_t	fatps = 0, probes = 0, losings = 0;

	tag = v4_port_tag(lport);
	if (!it->fp) {
		it->fp = ctl->fat->port[tag & ctl->fat->mask];
		it->slot_idx = 0;
	}
	fp  = it->fp;

	while (fp) {
		uint32_t	inuse = fp->inuse;

		++fatps;

		for (i = it->slot_idx; inuse && i < fatp_ntags(); ++i) {
			uint32_t	idx;

			if (!(inuse & (1 << i)))
				continue;

			inuse &= ~0 << i;

			if (i < it->slot_idx)
				continue;

			++vtw_stats.probe[1];
			++probes;

			idx = fp->tag[i] ^ tag ^ fatp_xtra[i];
			vtw = vtw_from_index(ctl, idx);

			if (!vtw) {
				/* Hopefully fast path.
				 */
				continue;
			}

			v4 = (void*)vtw;

			if (vtw_alive(vtw)
			    && vtw->port_key == fatp_key(ctl->fat, fp, i)
			    && v4->lport == lport) {
				++vtw_stats.hit[1];

				it->slot_idx = i + 1;

				goto out;
			} else if (vtw_alive(vtw)) {
				++vtw_stats.losing[1];
				++losings;

				db_trace(KTR_VTW
					 , (vtw, "vtw:!mis"
					    " port %8.8x:%4.4x %8.8x:%4.4x"
					    " key %x port %x"
					    , v4->faddr, v4->fport
					    , v4->laddr, v4->lport
					    , vtw->key
					    , lport));
			} else {
				/* Really losing here.  We are coming
				 * up with references to free entries.
				 * Might find it better to use
				 * traditional, or need another
				 * add-hockery.  The other add-hockery
				 * would be to pul more into into the
				 * cache line to reject the false
				 * hits.
				 */
				++vtw_stats.losing[1];
				++losings;
				db_trace(KTR_VTW
					 , (fp, "vtw:!mis port %x"
					    " - free entry idx %x vtw %p"
					    , lport
					    , idx_decode(ctl, idx)
					    , vtw));
			}
		}

		if (fp->nxt) {
			it->fp = fp = fatp_next(ctl->fat, fp);
			it->slot_idx = 0;
		} else {
			it->fp = 0;
			break;
		}
	}
	++vtw_stats.miss[1];

	vtw = 0;
out:
	if (fatps > vtw_stats.max_chain[1])
		vtw_stats.max_chain[1] = fatps;
	if (probes > vtw_stats.max_probe[1])
		vtw_stats.max_probe[1] = probes;
	if (losings > vtw_stats.max_loss[1])
		vtw_stats.max_loss[1] = losings;

	return vtw;
}

/*!\brief port iterator
 */
static vtw_t *
vtw_next_port_v6(struct tcp_ports_iterator *it)
{
	vtw_ctl_t	*ctl = it->ctl;
	vtw_v6_t	*v6;
	vtw_t		*vtw;
	uint32_t	tag;
	uint16_t	lport = it->port;
	fatp_t		*fp;
	int		i;
	uint32_t	fatps = 0, probes = 0, losings = 0;

	tag = v6_port_tag(lport);
	if (!it->fp) {
		it->fp = ctl->fat->port[tag & ctl->fat->mask];
		it->slot_idx = 0;
	}
	fp  = it->fp;

	while (fp) {
		uint32_t	inuse = fp->inuse;

		++fatps;

		for (i = it->slot_idx; inuse && i < fatp_ntags(); ++i) {
			uint32_t	idx;

			if (!(inuse & (1 << i)))
				continue;

			inuse &= ~0 << i;

			if (i < it->slot_idx)
				continue;

			++vtw_stats.probe[1];
			++probes;

			idx = fp->tag[i] ^ tag ^ fatp_xtra[i];
			vtw = vtw_from_index(ctl, idx);

			if (!vtw) {
				/* Hopefully fast path.
				 */
				continue;
			}

			v6 = (void*)vtw;

			db_trace(KTR_VTW
				 , (vtw, "vtw: i %x idx %x fp->tag %x"
				    " tag %x xtra %x"
				    , i, idx_decode(ctl, idx)
				    , fp->tag[i], tag, fatp_xtra[i]));

			if (vtw_alive(vtw)
			    && vtw->port_key == fatp_key(ctl->fat, fp, i)
			    && v6->lport == lport) {
				++vtw_stats.hit[1];

				db_trace(KTR_VTW
					 , (fp, "vtw: nxt port %P - %4.4x"
					    " idx %x key %x"
					    , lport, lport
					    , idx_decode(ctl, idx), vtw->key));

				it->slot_idx = i + 1;
				goto out;
			} else if (vtw_alive(vtw)) {
				++vtw_stats.losing[1];

				db_trace(KTR_VTW
					 , (vtw, "vtw:!mis port %6A:%4.4x"
					    " %6A:%4.4x key %x port %x"
					    , db_store(&v6->faddr
						       , sizeof (v6->faddr))
					    , v6->fport
					    , db_store(&v6->laddr
						       , sizeof (v6->faddr))
					    , v6->lport
					    , vtw->key
					    , lport));
			} else {
				/* Really losing here.  We are coming
				 * up with references to free entries.
				 * Might find it better to use
				 * traditional, or need another
				 * add-hockery.  The other add-hockery
				 * would be to pul more into into the
				 * cache line to reject the false
				 * hits.
				 */
				++vtw_stats.losing[1];
				++losings;

				db_trace(KTR_VTW
					 , (fp
					    , "vtw:!mis port %x"
					    " - free entry idx %x vtw %p"
					    , lport, idx_decode(ctl, idx)
					    , vtw));
			}
		}

		if (fp->nxt) {
			it->fp = fp = fatp_next(ctl->fat, fp);
			it->slot_idx = 0;
		} else {
			it->fp = 0;
			break;
		}
	}
	++vtw_stats.miss[1];

	vtw = 0;
out:
	if (fatps > vtw_stats.max_chain[1])
		vtw_stats.max_chain[1] = fatps;
	if (probes > vtw_stats.max_probe[1])
		vtw_stats.max_probe[1] = probes;
	if (losings > vtw_stats.max_loss[1])
		vtw_stats.max_loss[1] = losings;

	return vtw;
}

/*!\brief initialise the VTW allocation arena
 *
 * There are 1+3 allocation classes:
 *	0	classless
 *	{1,2,3}	MSL-class based allocation
 *
 * The allocation arenas are all initialised.  Classless gets all the
 * space.  MSL-class based divides the arena, so that allocation
 * within a class can proceed without having to consider entries
 * (aka: cache lines) from different classes.
 *
 * Usually, we are completely classless or class-based, but there can be
 * transition periods, corresponding to dynamic adjustments in the config
 * by the operator.
 */
static void
vtw_init(fatp_ctl_t *fat, vtw_ctl_t *ctl, const uint32_t n, vtw_t *ctl_base_v)
{
	int class_n, i;
	vtw_t	*base;

	ctl->base.v = ctl_base_v;

	if (ctl->is_v4) {
		ctl->lim.v4    = ctl->base.v4 + n - 1;
		ctl->alloc.v4  = ctl->base.v4;
	} else {
		ctl->lim.v6    = ctl->base.v6 + n - 1;
		ctl->alloc.v6  = ctl->base.v6;
	}

	ctl->nfree  = n;
	ctl->ctl    = ctl;

	ctl->idx_bits = 32;
	for (ctl->idx_mask = ~0; (ctl->idx_mask & (n-1)) == n-1; ) {
		ctl->idx_mask >>= 1;
		ctl->idx_bits  -= 1;
	}

	ctl->idx_mask <<= 1;
	ctl->idx_mask  |= 1;
	ctl->idx_bits  += 1;

	ctl->fat = fat;
	fat->vtw = ctl;

	/* Divide the resources equally amongst the classes.
	 * This is not optimal, as the different classes
	 * arrive and leave at different rates, but it is
	 * the best I can do for now.
	 */
	class_n = n / (VTW_NCLASS-1);
	base    = ctl->base.v;

	for (i = 1; i < VTW_NCLASS; ++i) {
		int j;

		ctl[i] = ctl[0];
		ctl[i].clidx = i;

		ctl[i].base.v = base;
		ctl[i].alloc  = ctl[i].base;

		for (j = 0; j < class_n - 1; ++j) {
			if (tcp_msl_enable)
				base->msl_class = i;
			base = vtw_next(ctl, base);
		}

		ctl[i].lim.v = base;
		base = vtw_next(ctl, base);
		ctl[i].nfree = class_n;
	}

	vtw_debug_init();
}

/*!\brief	map class to TCP MSL
 */
static inline uint32_t
class_to_msl(int msl_class)
{
	switch (msl_class) {
	case 0:
	case 1:
		return tcp_msl_remote ? tcp_msl_remote : (TCPTV_MSL >> 0);
	case 2:
		return tcp_msl_local ? tcp_msl_local : (TCPTV_MSL >> 1);
	default:
		return tcp_msl_loop ? tcp_msl_loop : (TCPTV_MSL >> 2);
	}
}

/*!\brief	map TCP MSL to class
 */
static inline uint32_t
msl_to_class(int msl)
{
	if (tcp_msl_enable) {
		if (msl <= (tcp_msl_loop ? tcp_msl_loop : (TCPTV_MSL >> 2)))
			return 1+2;
		if (msl <= (tcp_msl_local ? tcp_msl_local : (TCPTV_MSL >> 1)))
			return 1+1;
		return 1;
	}
	return 0;
}

/*!\brief allocate a vtw entry
 */
static inline vtw_t *
vtw_alloc(vtw_ctl_t *ctl)
{
	vtw_t	*vtw	= 0;
	int	stuck	= 0;
	int	avail	= ctl ? (ctl->nalloc + ctl->nfree) : 0;
	int	msl;

	KASSERT(mutex_owned(softnet_lock));

	/* If no resources, we will not get far.
	 */
	if (!ctl || !ctl->base.v4 || avail <= 0)
		return 0;

	/* Obtain a free one.
	 */
	while (!ctl->nfree) {
		vtw_age(ctl, 0);

		if (++stuck > avail) {
			/* When in transition between
			 * schemes (classless, classed) we
			 * can be stuck having to await the
			 * expiration of cross-allocated entries.
			 *
			 * Returning zero means we will fall back to the
			 * traditional TIME_WAIT handling, except in the
			 * case of a re-shed, in which case we cannot
			 * perform the reshecd, but will retain the extant
			 * entry.
			 */
			db_trace(KTR_VTW
				 , (ctl, "vtw:!none free in class %x %x/%x"
				    , ctl->clidx
				    , ctl->nalloc, ctl->nfree));

			return 0;
		}
	}

	vtw = ctl->alloc.v;

	if (vtw->msl_class != ctl->clidx) {
		/* Usurping rules:
		 * 	0 -> {1,2,3} or {1,2,3} -> 0
		 */
		KASSERT(!vtw->msl_class || !ctl->clidx);

		if (vtw->hashed || vtw->expire.tv_sec) {
		    /* As this is owned by some other class,
		     * we must wait for it to expire it.
		     * This will only happen on class/classless
		     * transitions, which are guaranteed to progress
		     * to completion in small finite time, barring bugs.
		     */
		    db_trace(KTR_VTW
			     , (ctl, "vtw:!%p class %x!=%x %x:%x%s"
				, vtw, vtw->msl_class, ctl->clidx
				, vtw->expire.tv_sec
				, vtw->expire.tv_usec
				, vtw->hashed ? " hashed" : ""));

		    return 0;
		}

		db_trace(KTR_VTW
			 , (ctl, "vtw:!%p usurped from %x to %x"
			    , vtw, vtw->msl_class, ctl->clidx));

		vtw->msl_class = ctl->clidx;
	}

	if (vtw_alive(vtw)) {
		KASSERT(0 && "next free not free");
		return 0;
	}

	/* Advance allocation poiter.
	 */
	ctl->alloc.v = vtw_next(ctl, vtw);

	--ctl->nfree;
	++ctl->nalloc;

	msl = (2 * class_to_msl(ctl->clidx) * 1000) / PR_SLOWHZ;	// msec

	/* mark expiration
	 */
	getmicrouptime(&vtw->expire);

	/* Move expiration into the future.
	 */
	vtw->expire.tv_sec  += msl / 1000;
	vtw->expire.tv_usec += 1000 * (msl % 1000);

	while (vtw->expire.tv_usec >= 1000*1000) {
		vtw->expire.tv_usec -= 1000*1000;
		vtw->expire.tv_sec  += 1;
	}

	if (!ctl->oldest.v)
		ctl->oldest.v = vtw;

	return vtw;
}

/*!\brief expiration
 */
static int
vtw_age(vtw_ctl_t *ctl, struct timeval *_when)
{
	vtw_t	*vtw;
	struct timeval then, *when = _when;
	int	maxtries = 0;

	if (!ctl->oldest.v) {
		KASSERT(!ctl->nalloc);
		return 0;
	}

	for (vtw = ctl->oldest.v; vtw && ctl->nalloc; ) {
		if (++maxtries > ctl->nalloc)
			break;

		if (vtw->msl_class != ctl->clidx) {
			db_trace(KTR_VTW
				 , (vtw, "vtw:!age class mismatch %x != %x"
				    , vtw->msl_class, ctl->clidx));
			/* XXXX
			 * See if the appropriate action is to skip to the next.
			 * XXXX
			 */
			ctl->oldest.v = vtw = vtw_next(ctl, vtw);
			continue;
		}
		if (!when) {
			/* Latch oldest timeval if none specified.
			 */
			then = vtw->expire;
			when = &then;
		}

		if (!timercmp(&vtw->expire, when, <=))
			break;

		db_trace(KTR_VTW
			 , (vtw, "vtw: expire %x %8.8x:%8.8x %x/%x"
			    , ctl->clidx
			    , vtw->expire.tv_sec
			    , vtw->expire.tv_usec
			    , ctl->nalloc
			    , ctl->nfree));

		if (!_when)
			++vtw_stats.kill;

		vtw_del(ctl, vtw);
		vtw = ctl->oldest.v;
	}

	return ctl->nalloc;	// # remaining allocated
}

static callout_t vtw_cs;

/*!\brief notice the passage of time.
 * It seems to be getting faster.  What happened to the year?
 */
static void
vtw_tick(void *arg)
{
	struct timeval now;
	int i, cnt = 0;

	getmicrouptime(&now);

	db_trace(KTR_VTW, (arg, "vtk: tick - now %8.8x:%8.8x"
			   , now.tv_sec, now.tv_usec));

	mutex_enter(softnet_lock);

	for (i = 0; i < VTW_NCLASS; ++i) {
		cnt += vtw_age(&vtw_tcpv4[i], &now);
		cnt += vtw_age(&vtw_tcpv6[i], &now);
	}

	/* Keep ticks coming while we need them.
	 */
	if (cnt)
		callout_schedule(&vtw_cs, hz / 5);
	else {
		tcp_vtw_was_enabled = 0;
		tcbtable.vestige    = 0;
	}
	mutex_exit(softnet_lock);
}

/* in_pcblookup_ports assist for handling vestigial entries.
 */
static void *
tcp_init_ports_v4(struct in_addr addr, u_int port, int wild)
{
	struct tcp_ports_iterator *it = &tcp_ports_iterator_v4;

	bzero(it, sizeof (*it));

	/* Note: the reference to vtw_tcpv4[0] is fine.
	 * We do not need per-class iteration.  We just
	 * need to get to the fat, and there is one
	 * shared fat.
	 */
	if (vtw_tcpv4[0].fat) {
		it->addr.v4 = addr;
		it->port = port;
		it->wild = !!wild;
		it->ctl  = &vtw_tcpv4[0];

		++vtw_stats.look[1];
	}

	return it;
}

/*!\brief export an IPv4 vtw.
 */
static int
vtw_export_v4(vtw_ctl_t *ctl, vtw_t *vtw, vestigial_inpcb_t *res)
{
	vtw_v4_t	*v4 = (void*)vtw;

	bzero(res, sizeof (*res));

	if (ctl && vtw) {
		if (!ctl->clidx && vtw->msl_class)
			ctl += vtw->msl_class;
		else
			KASSERT(ctl->clidx == vtw->msl_class);

		res->valid = 1;
		res->v4    = 1;

		res->faddr.v4.s_addr = v4->faddr;
		res->laddr.v4.s_addr = v4->laddr;
		res->fport	= v4->fport;
		res->lport	= v4->lport;
		res->vtw	= vtw;		// netlock held over call(s)
		res->ctl	= ctl;
		res->reuse_addr = vtw->reuse_addr;
		res->reuse_port = vtw->reuse_port;
		res->snd_nxt    = vtw->snd_nxt;
		res->rcv_nxt	= vtw->rcv_nxt;
		res->rcv_wnd	= vtw->rcv_wnd;
		res->uid	= vtw->uid;
	}

	return res->valid;
}

/*!\brief return next port in the port iterator.  yowza.
 */
static int
tcp_next_port_v4(void *arg, struct vestigial_inpcb *res)
{
	struct tcp_ports_iterator *it = arg;
	vtw_t		*vtw = 0;

	if (it->ctl)
		vtw = vtw_next_port_v4(it);

	if (!vtw)
		it->ctl = 0;

	return vtw_export_v4(it->ctl, vtw, res);
}

static int
tcp_lookup_v4(struct in_addr faddr, uint16_t fport,
              struct in_addr laddr, uint16_t lport,
	      struct vestigial_inpcb *res)
{
	vtw_t		*vtw;
	vtw_ctl_t	*ctl;


	db_trace(KTR_VTW
		 , (res, "vtw: lookup %A:%P %A:%P"
		    , faddr, fport
		    , laddr, lport));

	vtw = vtw_lookup_hash_v4((ctl = &vtw_tcpv4[0])
				 , faddr.s_addr, fport
				 , laddr.s_addr, lport, 0);

	return vtw_export_v4(ctl, vtw, res);
}

/* in_pcblookup_ports assist for handling vestigial entries.
 */
static void *
tcp_init_ports_v6(const struct in6_addr *addr, u_int port, int wild)
{
	struct tcp_ports_iterator *it = &tcp_ports_iterator_v6;

	bzero(it, sizeof (*it));

	/* Note: the reference to vtw_tcpv6[0] is fine.
	 * We do not need per-class iteration.  We just
	 * need to get to the fat, and there is one
	 * shared fat.
	 */
	if (vtw_tcpv6[0].fat) {
		it->addr.v6 = *addr;
		it->port = port;
		it->wild = !!wild;
		it->ctl  = &vtw_tcpv6[0];

		++vtw_stats.look[1];
	}

	return it;
}

/*!\brief export an IPv6 vtw.
 */
static int
vtw_export_v6(vtw_ctl_t *ctl, vtw_t *vtw, vestigial_inpcb_t *res)
{
	vtw_v6_t	*v6 = (void*)vtw;

	bzero(res, sizeof (*res));

	if (ctl && vtw) {
		if (!ctl->clidx && vtw->msl_class)
			ctl += vtw->msl_class;
		else
			KASSERT(ctl->clidx == vtw->msl_class);

		res->valid = 1;
		res->v4    = 0;

		res->faddr.v6	= v6->faddr;
		res->laddr.v6	= v6->laddr;
		res->fport	= v6->fport;
		res->lport	= v6->lport;
		res->vtw	= vtw;		// netlock held over call(s)
		res->ctl	= ctl;

		res->v6only	= vtw->v6only;
		res->reuse_addr = vtw->reuse_addr;
		res->reuse_port = vtw->reuse_port;

		res->snd_nxt    = vtw->snd_nxt;
		res->rcv_nxt	= vtw->rcv_nxt;
		res->rcv_wnd	= vtw->rcv_wnd;
		res->uid	= vtw->uid;
	}

	return res->valid;
}

static int
tcp_next_port_v6(void *arg, struct vestigial_inpcb *res)
{
	struct tcp_ports_iterator *it = arg;
	vtw_t		*vtw = 0;

	if (it->ctl)
		vtw = vtw_next_port_v6(it);

	if (!vtw)
		it->ctl = 0;

	return vtw_export_v6(it->ctl, vtw, res);
}

static int
tcp_lookup_v6(const struct in6_addr *faddr, uint16_t fport,
              const struct in6_addr *laddr, uint16_t lport,
	      struct vestigial_inpcb *res)
{
	vtw_ctl_t	*ctl;
	vtw_t		*vtw;

	db_trace(KTR_VTW
		 , (res, "vtw: lookup %6A:%P %6A:%P"
		    , db_store(faddr, sizeof (*faddr)), fport
		    , db_store(laddr, sizeof (*laddr)), lport));

	vtw = vtw_lookup_hash_v6((ctl = &vtw_tcpv6[0])
				 , faddr, fport
				 , laddr, lport, 0);

	return vtw_export_v6(ctl, vtw, res);
}

static vestigial_hooks_t tcp_hooks = {
	.init_ports4	= tcp_init_ports_v4,
	.next_port4	= tcp_next_port_v4,
	.lookup4	= tcp_lookup_v4,
	.init_ports6	= tcp_init_ports_v6,
	.next_port6	= tcp_next_port_v6,
	.lookup6	= tcp_lookup_v6,
};

static bool
vtw_select(int af, fatp_ctl_t **fatp, vtw_ctl_t **ctlp)
{
	fatp_ctl_t	*fat;
	vtw_ctl_t	*ctl;

	switch (af) {
	case AF_INET:
		fat = &fat_tcpv4;
		ctl = &vtw_tcpv4[0];
		break;
	case AF_INET6:
		fat = &fat_tcpv6;
		ctl = &vtw_tcpv6[0];
		break;
	default:
		return false;
	}
	if (fatp != NULL)
		*fatp = fat;
	if (ctlp != NULL)
		*ctlp = ctl;
	return true;
}

/*!\brief	initialize controlling instance
 */
static int
vtw_control_init(int af)
{
	fatp_ctl_t	*fat;
	vtw_ctl_t	*ctl;
	fatp_t		*fat_base;
	fatp_t		**fat_hash;
	vtw_t		*ctl_base_v;
	uint32_t	n, m;
	size_t sz;

	KASSERT(powerof2(tcp_vtw_entries));

	if (!vtw_select(af, &fat, &ctl))
		return EAFNOSUPPORT;

	if (fat->hash != NULL) {
		KASSERT(fat->base != NULL && ctl->base.v != NULL);
		return 0;
	}

	/* Allocate 10% more capacity in the fat pointers.
	 * We should only need ~#hash additional based on
	 * how they age, but TIME_WAIT assassination could cause
	 * sparse fat pointer utilisation.
	 */
	m = 512;
	n = 2*m + (11 * (tcp_vtw_entries / fatp_ntags())) / 10;
	sz = (ctl->is_v4 ? sizeof(vtw_v4_t) : sizeof(vtw_v6_t));

	fat_hash = kmem_zalloc(2*m * sizeof(fatp_t *), KM_NOSLEEP);

	if (fat_hash == NULL) {
		printf("%s: could not allocate %zu bytes for "
		    "hash anchors", __func__, 2*m * sizeof(fatp_t *));
		return ENOMEM;
	}

	fat_base = kmem_zalloc(2*n * sizeof(fatp_t), KM_NOSLEEP);

	if (fat_base == NULL) {
		kmem_free(fat_hash, 2*m * sizeof (fatp_t *));
		printf("%s: could not allocate %zu bytes for "
		    "fatp_t array", __func__, 2*n * sizeof(fatp_t));
		return ENOMEM;
	}

	ctl_base_v = kmem_zalloc(tcp_vtw_entries * sz, KM_NOSLEEP);

	if (ctl_base_v == NULL) {
		kmem_free(fat_hash, 2*m * sizeof (fatp_t *));
		kmem_free(fat_base, 2*n * sizeof(fatp_t));
		printf("%s: could not allocate %zu bytes for "
		    "vtw_t array", __func__, tcp_vtw_entries * sz);
		return ENOMEM;
	}

	fatp_init(fat, n, m, fat_base, fat_hash);

	vtw_init(fat, ctl, tcp_vtw_entries, ctl_base_v);

	return 0;
}

/*!\brief	select controlling instance
 */
static vtw_ctl_t *
vtw_control(int af, uint32_t msl)
{
	fatp_ctl_t	*fat;
	vtw_ctl_t	*ctl;
	int		msl_class = msl_to_class(msl);

	if (!vtw_select(af, &fat, &ctl))
		return NULL;

	if (!fat->base || !ctl->base.v)
		return NULL;

	if (!tcp_vtw_was_enabled) {
		/* This guarantees is timer ticks until we no longer need them.
		 */
		tcp_vtw_was_enabled = 1;

		callout_schedule(&vtw_cs, hz / 5);

		tcbtable.vestige = &tcp_hooks;
	}

	return ctl + msl_class;
}

/*!\brief	add TCP pcb to vestigial timewait
 */
int
vtw_add(int af, struct tcpcb *tp)
{
#ifdef VTW_DEBUG
	int		enable;
#endif
	vtw_ctl_t	*ctl;
	vtw_t		*vtw;

	KASSERT(mutex_owned(softnet_lock));

	ctl = vtw_control(af, tp->t_msl);
	if (!ctl)
		return 0;

#ifdef VTW_DEBUG
	enable = (af == AF_INET) ? tcp4_vtw_enable : tcp6_vtw_enable;
#endif

	vtw = vtw_alloc(ctl);

	if (vtw) {
		vtw->snd_nxt = tp->snd_nxt;
		vtw->rcv_nxt = tp->rcv_nxt;

		switch (af) {
		case AF_INET: {
			struct inpcb	*inp = tp->t_inpcb;
			vtw_v4_t	*v4  = (void*)vtw;

			v4->faddr = inp->inp_faddr.s_addr;
			v4->laddr = inp->inp_laddr.s_addr;
			v4->fport = inp->inp_fport;
			v4->lport = inp->inp_lport;

			vtw->reuse_port = !!(inp->inp_socket->so_options
					     & SO_REUSEPORT);
			vtw->reuse_addr = !!(inp->inp_socket->so_options
					     & SO_REUSEADDR);
			vtw->v6only	= 0;
			vtw->uid	= inp->inp_socket->so_uidinfo->ui_uid;

			vtw_inshash_v4(ctl, vtw);


#ifdef VTW_DEBUG
			/* Immediate lookup (connected and port) to
			 * ensure at least that works!
			 */
			if (enable & 4) {
				KASSERT(vtw_lookup_hash_v4
					(ctl
					 , inp->inp_faddr.s_addr, inp->inp_fport
					 , inp->inp_laddr.s_addr, inp->inp_lport
					 , 0)
					== vtw);
				KASSERT(vtw_lookup_hash_v4
					(ctl
					 , inp->inp_faddr.s_addr, inp->inp_fport
					 , inp->inp_laddr.s_addr, inp->inp_lport
					 , 1));
			}
			/* Immediate port iterator functionality check: not wild
			 */
			if (enable & 8) {
				struct tcp_ports_iterator *it;
				struct vestigial_inpcb res;
				int cnt = 0;

				it = tcp_init_ports_v4(inp->inp_laddr
						       , inp->inp_lport, 0);

				while (tcp_next_port_v4(it, &res)) {
					++cnt;
				}
				KASSERT(cnt);
			}
			/* Immediate port iterator functionality check: wild
			 */
			if (enable & 16) {
				struct tcp_ports_iterator *it;
				struct vestigial_inpcb res;
				struct in_addr any;
				int cnt = 0;

				any.s_addr = htonl(INADDR_ANY);

				it = tcp_init_ports_v4(any, inp->inp_lport, 1);

				while (tcp_next_port_v4(it, &res)) {
					++cnt;
				}
				KASSERT(cnt);
			}
#endif /* VTW_DEBUG */
			break;
		}

		case AF_INET6: {
			struct in6pcb	*inp = tp->t_in6pcb;
			vtw_v6_t	*v6  = (void*)vtw;

			v6->faddr = inp->in6p_faddr;
			v6->laddr = inp->in6p_laddr;
			v6->fport = inp->in6p_fport;
			v6->lport = inp->in6p_lport;

			vtw->reuse_port = !!(inp->in6p_socket->so_options
					     & SO_REUSEPORT);
			vtw->reuse_addr = !!(inp->in6p_socket->so_options
					     & SO_REUSEADDR);
			vtw->v6only	= !!(inp->in6p_flags
					     & IN6P_IPV6_V6ONLY);
			vtw->uid	= inp->in6p_socket->so_uidinfo->ui_uid;

			vtw_inshash_v6(ctl, vtw);
#ifdef VTW_DEBUG
			/* Immediate lookup (connected and port) to
			 * ensure at least that works!
			 */
			if (enable & 4) {
				KASSERT(vtw_lookup_hash_v6(ctl
					 , &inp->in6p_faddr, inp->in6p_fport
					 , &inp->in6p_laddr, inp->in6p_lport
					 , 0)
					== vtw);
				KASSERT(vtw_lookup_hash_v6
					(ctl
					 , &inp->in6p_faddr, inp->in6p_fport
					 , &inp->in6p_laddr, inp->in6p_lport
					 , 1));
			}
			/* Immediate port iterator functionality check: not wild
			 */
			if (enable & 8) {
				struct tcp_ports_iterator *it;
				struct vestigial_inpcb res;
				int cnt = 0;

				it = tcp_init_ports_v6(&inp->in6p_laddr
						       , inp->in6p_lport, 0);

				while (tcp_next_port_v6(it, &res)) {
					++cnt;
				}
				KASSERT(cnt);
			}
			/* Immediate port iterator functionality check: wild
			 */
			if (enable & 16) {
				struct tcp_ports_iterator *it;
				struct vestigial_inpcb res;
				static struct in6_addr any = IN6ADDR_ANY_INIT;
				int cnt = 0;

				it = tcp_init_ports_v6(&any
						       , inp->in6p_lport, 1);

				while (tcp_next_port_v6(it, &res)) {
					++cnt;
				}
				KASSERT(cnt);
			}
#endif /* VTW_DEBUG */
			break;
		}
		}

		tcp_canceltimers(tp);
		tp = tcp_close(tp);
		KASSERT(!tp);

		return 1;
	}

	return 0;
}

/*!\brief	restart timer for vestigial time-wait entry
 */
static void
vtw_restart_v4(vestigial_inpcb_t *vp)
{
	vtw_v4_t	copy = *(vtw_v4_t*)vp->vtw;
	vtw_t		*vtw;
	vtw_t		*cp  = &copy.common;
	vtw_ctl_t	*ctl;

	KASSERT(mutex_owned(softnet_lock));

	db_trace(KTR_VTW
		 , (vp->vtw, "vtw: restart %A:%P %A:%P"
		    , vp->faddr.v4.s_addr, vp->fport
		    , vp->laddr.v4.s_addr, vp->lport));

	/* Class might have changed, so have a squiz.
	 */
	ctl = vtw_control(AF_INET, class_to_msl(cp->msl_class));
	vtw = vtw_alloc(ctl);

	if (vtw) {
		vtw_v4_t	*v4  = (void*)vtw;

		/* Safe now to unhash the old entry
		 */
		vtw_del(vp->ctl, vp->vtw);

		vtw->snd_nxt = cp->snd_nxt;
		vtw->rcv_nxt = cp->rcv_nxt;

		v4->faddr = copy.faddr;
		v4->laddr = copy.laddr;
		v4->fport = copy.fport;
		v4->lport = copy.lport;

		vtw->reuse_port = cp->reuse_port;
		vtw->reuse_addr = cp->reuse_addr;
		vtw->v6only	= 0;
		vtw->uid	= cp->uid;

		vtw_inshash_v4(ctl, vtw);
	}

	vp->valid = 0;
}

/*!\brief	restart timer for vestigial time-wait entry
 */
static void
vtw_restart_v6(vestigial_inpcb_t *vp)
{
	vtw_v6_t	copy = *(vtw_v6_t*)vp->vtw;
	vtw_t		*vtw;
	vtw_t		*cp  = &copy.common;
	vtw_ctl_t	*ctl;

	KASSERT(mutex_owned(softnet_lock));

	db_trace(KTR_VTW
		 , (vp->vtw, "vtw: restart %6A:%P %6A:%P"
		    , db_store(&vp->faddr.v6, sizeof (vp->faddr.v6))
		    , vp->fport
		    , db_store(&vp->laddr.v6, sizeof (vp->laddr.v6))
		    , vp->lport));

	/* Class might have changed, so have a squiz.
	 */
	ctl = vtw_control(AF_INET6, class_to_msl(cp->msl_class));
	vtw = vtw_alloc(ctl);

	if (vtw) {
		vtw_v6_t	*v6  = (void*)vtw;

		/* Safe now to unhash the old entry
		 */
		vtw_del(vp->ctl, vp->vtw);

		vtw->snd_nxt = cp->snd_nxt;
		vtw->rcv_nxt = cp->rcv_nxt;

		v6->faddr = copy.faddr;
		v6->laddr = copy.laddr;
		v6->fport = copy.fport;
		v6->lport = copy.lport;

		vtw->reuse_port = cp->reuse_port;
		vtw->reuse_addr = cp->reuse_addr;
		vtw->v6only	= cp->v6only;
		vtw->uid	= cp->uid;

		vtw_inshash_v6(ctl, vtw);
	}

	vp->valid = 0;
}

/*!\brief	restart timer for vestigial time-wait entry
 */
void
vtw_restart(vestigial_inpcb_t *vp)
{
	if (!vp || !vp->valid)
		return;

	if (vp->v4)
		vtw_restart_v4(vp);
	else
		vtw_restart_v6(vp);
}

int
sysctl_tcp_vtw_enable(SYSCTLFN_ARGS)
{  
	int en, rc;
	struct sysctlnode node;

	node = *rnode;
	en = *(int *)rnode->sysctl_data;
	node.sysctl_data = &en;

	rc = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (rc != 0 || newp == NULL)
		return rc;

	if (rnode->sysctl_data != &tcp4_vtw_enable &&
	    rnode->sysctl_data != &tcp6_vtw_enable)
		rc = ENOENT;
	else if ((en & 1) == 0)
		rc = 0;
	else if (rnode->sysctl_data == &tcp4_vtw_enable)
		rc = vtw_control_init(AF_INET);
	else /* rnode->sysctl_data == &tcp6_vtw_enable */
		rc = vtw_control_init(AF_INET6);

	if (rc == 0)
		*(int *)rnode->sysctl_data = en;

	return rc;
}

int
vtw_earlyinit(void)
{
	int i, rc;

	callout_init(&vtw_cs, 0);
	callout_setfunc(&vtw_cs, vtw_tick, 0);

	for (i = 0; i < VTW_NCLASS; ++i) {
		vtw_tcpv4[i].is_v4 = 1;
		vtw_tcpv6[i].is_v6 = 1;
	}

	if ((tcp4_vtw_enable & 1) != 0 &&
	    (rc = vtw_control_init(AF_INET)) != 0)
		return rc;

	if ((tcp6_vtw_enable & 1) != 0 &&
	    (rc = vtw_control_init(AF_INET6)) != 0)
		return rc;

	return 0;
}

#ifdef VTW_DEBUG
#include <sys/syscallargs.h>
#include <sys/sysctl.h>

/*!\brief	add lalp, fafp entries for debug
 */
int
vtw_debug_add(int af, sin_either_t *la, sin_either_t *fa, int msl, int msl_class)
{
	vtw_ctl_t	*ctl;
	vtw_t		*vtw;

	ctl = vtw_control(af, msl ? msl : class_to_msl(msl_class));
	if (!ctl)
		return 0;

	vtw = vtw_alloc(ctl);

	if (vtw) {
		vtw->snd_nxt = 0;
		vtw->rcv_nxt = 0;

		switch (af) {
		case AF_INET: {
			vtw_v4_t	*v4  = (void*)vtw;

			v4->faddr = fa->sin_addr.v4.s_addr;
			v4->laddr = la->sin_addr.v4.s_addr;
			v4->fport = fa->sin_port;
			v4->lport = la->sin_port;

			vtw->reuse_port = 1;
			vtw->reuse_addr = 1;
			vtw->v6only	= 0;
			vtw->uid	= 0;

			vtw_inshash_v4(ctl, vtw);
			break;
		}

		case AF_INET6: {
			vtw_v6_t	*v6  = (void*)vtw;

			v6->faddr = fa->sin_addr.v6;
			v6->laddr = la->sin_addr.v6;

			v6->fport = fa->sin_port;
			v6->lport = la->sin_port;

			vtw->reuse_port = 1;
			vtw->reuse_addr = 1;
			vtw->v6only	= 0;
			vtw->uid	= 0;

			vtw_inshash_v6(ctl, vtw);
			break;
		}

		default:
			break;
		}

		return 1;
	}

	return 0;
}

static int vtw_syscall = 0;

static int
vtw_debug_process(vtw_sysargs_t *ap)
{
	struct vestigial_inpcb vestige;
	int	rc = 0;

	mutex_enter(softnet_lock);

	switch (ap->op) {
	case 0:		// insert
		vtw_debug_add(ap->la.sin_family
			      , &ap->la
			      , &ap->fa
			      , TCPTV_MSL
			      , 0);
		break;

	case 1:		// lookup
	case 2:		// restart
		switch (ap->la.sin_family) {
		case AF_INET:
			if (tcp_lookup_v4(ap->fa.sin_addr.v4, ap->fa.sin_port,
					  ap->la.sin_addr.v4, ap->la.sin_port,
					  &vestige)) {
				if (ap->op == 2) {
					vtw_restart(&vestige);
				}
				rc = 0;
			} else
				rc = ESRCH;
			break;

		case AF_INET6:
			if (tcp_lookup_v6(&ap->fa.sin_addr.v6, ap->fa.sin_port,
					  &ap->la.sin_addr.v6, ap->la.sin_port,
					  &vestige)) {
				if (ap->op == 2) {
					vtw_restart(&vestige);
				}
				rc = 0;
			} else
				rc = ESRCH;
			break;
		default:
			rc = EINVAL;
		}
		break;

	default:
		rc = EINVAL;
	}

	mutex_exit(softnet_lock);
	return rc;
}

struct sys_vtw_args {
	syscallarg(const vtw_sysargs_t *) req;
	syscallarg(size_t) len;
};

static int
vtw_sys(struct lwp *l, const void *_, register_t *retval)
{
	const struct sys_vtw_args *uap = _;
	void	*buf;
	int	rc;
	size_t	len	= SCARG(uap, len);

	if (len != sizeof (vtw_sysargs_t))
		return EINVAL;

	buf = kmem_alloc(len, KM_SLEEP);
	if (!buf)
		return ENOMEM;

	rc = copyin(SCARG(uap, req), buf, len);
	if (!rc) {
		rc = vtw_debug_process(buf);
	}
	kmem_free(buf, len);

	return rc;
}

static void
vtw_sanity_check(void)
{
	vtw_ctl_t	*ctl;
	vtw_t		*vtw;
	int		i;
	int		n;

	for (i = 0; i < VTW_NCLASS; ++i) {
		ctl = &vtw_tcpv4[i];

		if (!ctl->base.v || ctl->nalloc)
			continue;

		for (n = 0, vtw = ctl->base.v; ; ) {
			++n;
			vtw = vtw_next(ctl, vtw);
			if (vtw == ctl->base.v)
				break;
		}
		db_trace(KTR_VTW
			 , (ctl, "sanity: class %x n %x nfree %x"
			    , i, n, ctl->nfree));

		KASSERT(n == ctl->nfree);
	}

	for (i = 0; i < VTW_NCLASS; ++i) {
		ctl = &vtw_tcpv6[i];

		if (!ctl->base.v || ctl->nalloc)
			continue;

		for (n = 0, vtw = ctl->base.v; ; ) {
			++n;
			vtw = vtw_next(ctl, vtw);
			if (vtw == ctl->base.v)
				break;
		}
		db_trace(KTR_VTW
			 , (ctl, "sanity: class %x n %x nfree %x"
			    , i, n, ctl->nfree));
		KASSERT(n == ctl->nfree);
	}
}
		
/*!\brief	Initialise debug support.
 */
static void
vtw_debug_init(void)
{
	int	i;

	vtw_sanity_check();

	if (vtw_syscall)
		return;

	for (i = 511; i; --i) {
		if (sysent[i].sy_call == sys_nosys) {
			sysent[i].sy_call    = vtw_sys;
			sysent[i].sy_narg    = 2;
			sysent[i].sy_argsize = sizeof (struct sys_vtw_args);
			sysent[i].sy_flags   = 0;

			vtw_syscall = i;
			break;
		}
	}
	if (i) {
		const struct sysctlnode *node;
		uint32_t	flags;

		flags = sysctl_root.sysctl_flags;

		sysctl_root.sysctl_flags |= CTLFLAG_READWRITE;
		sysctl_root.sysctl_flags &= ~CTLFLAG_PERMANENT;

		sysctl_createv(0, 0, 0, &node,
			       CTLFLAG_PERMANENT, CTLTYPE_NODE,
			       "koff",
			       SYSCTL_DESCR("Kernel Obscure Feature Finder"),
			       0, 0, 0, 0, CTL_CREATE, CTL_EOL);

		if (!node) {
			sysctl_createv(0, 0, 0, &node,
				       CTLFLAG_PERMANENT, CTLTYPE_NODE,
				       "koffka",
				       SYSCTL_DESCR("The Real(tm) Kernel"
						    " Obscure Feature Finder"),
				       0, 0, 0, 0, CTL_CREATE, CTL_EOL);
		}
		if (node) {
			sysctl_createv(0, 0, 0, 0,
				       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
				       CTLTYPE_INT, "vtw_debug_syscall",
				       SYSCTL_DESCR("vtw debug"
						    " system call number"),
				       0, 0, &vtw_syscall, 0, node->sysctl_num,
				       CTL_CREATE, CTL_EOL);
		}
		sysctl_root.sysctl_flags = flags;
	}
}
#else /* !VTW_DEBUG */
static void
vtw_debug_init(void)
{
	return;
}
#endif /* !VTW_DEBUG */
