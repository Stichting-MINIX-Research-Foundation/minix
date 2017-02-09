/*	$NetBSD: npf_nat.c,v 1.39 2014/12/30 19:11:44 christos Exp $	*/

/*-
 * Copyright (c) 2014 Mindaugas Rasiukevicius <rmind at netbsd org>
 * Copyright (c) 2010-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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
 * NPF network address port translation (NAPT) and other forms of NAT.
 * Described in RFC 2663, RFC 3022, etc.
 *
 * Overview
 *
 *	There are few mechanisms: NAT policy, port map and translation.
 *	NAT module has a separate ruleset, where rules contain associated
 *	NAT policy, thus flexible filter criteria can be used.
 *
 * Translation types
 *
 *	There are two types of translation: outbound (NPF_NATOUT) and
 *	inbound (NPF_NATIN).  It should not be confused with connection
 *	direction.  See npf_nat_which() for the description of how the
 *	addresses are rewritten.
 *
 *	It should be noted that bi-directional NAT is a combined outbound
 *	and inbound translation, therefore constructed as two policies.
 *
 * NAT policies and port maps
 *
 *	NAT (translation) policy is applied when a packet matches the rule.
 *	Apart from filter criteria, NAT policy has a translation IP address
 *	and associated port map.  Port map is a bitmap used to reserve and
 *	use unique TCP/UDP ports for translation.  Port maps are unique to
 *	the IP addresses, therefore multiple NAT policies with the same IP
 *	will share the same port map.
 *
 * Connections, translation entries and their life-cycle
 *
 *	NAT module relies on connection tracking module.  Each translated
 *	connection has an associated translation entry (npf_nat_t), which
 *	contains information used for backwards stream translation, i.e.
 *	original IP address with port and translation port, allocated from
 *	the port map.  Each NAT entry is associated with the policy, which
 *	contains translation IP address.  Allocated port is returned to the
 *	port map and NAT entry is destroyed when connection expires.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_nat.c,v 1.39 2014/12/30 19:11:44 christos Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>
#include <sys/bitops.h>
#include <sys/condvar.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/cprng.h>

#include <net/pfil.h>
#include <netinet/in.h>

#include "npf_impl.h"
#include "npf_conn.h"

/*
 * NPF portmap structure.
 */
typedef struct {
	u_int			p_refcnt;
	uint32_t		p_bitmap[0];
} npf_portmap_t;

/* Portmap range: [ 1024 .. 65535 ] */
#define	PORTMAP_FIRST		(1024)
#define	PORTMAP_SIZE		((65536 - PORTMAP_FIRST) / 32)
#define	PORTMAP_FILLED		((uint32_t)~0U)
#define	PORTMAP_MASK		(31)
#define	PORTMAP_SHIFT		(5)

#define	PORTMAP_MEM_SIZE	\
    (sizeof(npf_portmap_t) + (PORTMAP_SIZE * sizeof(uint32_t)))

/*
 * NAT policy structure.
 */
struct npf_natpolicy {
	kmutex_t		n_lock;
	LIST_HEAD(, npf_nat)	n_nat_list;
	volatile u_int		n_refcnt;
	npf_portmap_t *		n_portmap;
	uint64_t		n_id;

	/*
	 * Translation type, flags and address.  Optionally, prefix
	 * for the NPTv6 and translation port.  Translation algorithm
	 * and related data (for NPTv6, the adjustment value).
	 *
	 * NPF_NP_CMP_START mark starts here.
	 */
	int			n_type;
	u_int			n_flags;
	u_int			n_alen;
	npf_addr_t		n_taddr;
	npf_netmask_t		n_tmask;
	in_port_t		n_tport;
	u_int			n_algo;
	union {
		uint16_t	n_npt66_adj;
	};
};

#define	NPF_NP_CMP_START	offsetof(npf_natpolicy_t, n_type)
#define	NPF_NP_CMP_SIZE		(sizeof(npf_natpolicy_t) - NPF_NP_CMP_START)

/*
 * NAT translation entry for a connection.
 */
struct npf_nat {
	/* Associated NAT policy. */
	npf_natpolicy_t *	nt_natpolicy;

	/*
	 * Original address and port (for backwards translation).
	 * Translation port (for redirects).
	 */
	npf_addr_t		nt_oaddr;
	in_port_t		nt_oport;
	in_port_t		nt_tport;

	/* ALG (if any) associated with this NAT entry. */
	npf_alg_t *		nt_alg;
	uintptr_t		nt_alg_arg;

	LIST_ENTRY(npf_nat)	nt_entry;
	npf_conn_t *		nt_conn;
};

static pool_cache_t		nat_cache	__read_mostly;

/*
 * npf_nat_sys{init,fini}: initialise/destroy NAT subsystem structures.
 */

void
npf_nat_sysinit(void)
{
	nat_cache = pool_cache_init(sizeof(npf_nat_t), coherency_unit,
	    0, 0, "npfnatpl", NULL, IPL_NET, NULL, NULL, NULL);
	KASSERT(nat_cache != NULL);
}

void
npf_nat_sysfini(void)
{
	/* All NAT policies should already be destroyed. */
	pool_cache_destroy(nat_cache);
}

/*
 * npf_nat_newpolicy: create a new NAT policy.
 *
 * => Shares portmap if policy is on existing translation address.
 */
npf_natpolicy_t *
npf_nat_newpolicy(prop_dictionary_t natdict, npf_ruleset_t *rset)
{
	npf_natpolicy_t *np;
	prop_object_t obj;
	npf_portmap_t *pm;

	np = kmem_zalloc(sizeof(npf_natpolicy_t), KM_SLEEP);

	/* The translation type, flags and policy ID. */
	prop_dictionary_get_int32(natdict, "type", &np->n_type);
	prop_dictionary_get_uint32(natdict, "flags", &np->n_flags);
	prop_dictionary_get_uint64(natdict, "nat-policy", &np->n_id);

	/* Should be exclusively either inbound or outbound NAT. */
	if (((np->n_type == NPF_NATIN) ^ (np->n_type == NPF_NATOUT)) == 0) {
		goto err;
	}
	mutex_init(&np->n_lock, MUTEX_DEFAULT, IPL_SOFTNET);
	LIST_INIT(&np->n_nat_list);

	/* Translation IP, mask and port (if applicable). */
	obj = prop_dictionary_get(natdict, "nat-ip");
	np->n_alen = prop_data_size(obj);
	if (np->n_alen == 0 || np->n_alen > sizeof(npf_addr_t)) {
		goto err;
	}
	memcpy(&np->n_taddr, prop_data_data_nocopy(obj), np->n_alen);
	prop_dictionary_get_uint8(natdict, "nat-mask", &np->n_tmask);
	prop_dictionary_get_uint16(natdict, "nat-port", &np->n_tport);

	prop_dictionary_get_uint32(natdict, "nat-algo", &np->n_algo);
	switch (np->n_algo) {
	case NPF_ALGO_NPT66:
		prop_dictionary_get_uint16(natdict, "npt66-adj",
		    &np->n_npt66_adj);
		break;
	default:
		if (np->n_tmask != NPF_NO_NETMASK)
			goto err;
		break;
	}

	/* Determine if port map is needed. */
	np->n_portmap = NULL;
	if ((np->n_flags & NPF_NAT_PORTMAP) == 0) {
		/* No port map. */
		return np;
	}

	/*
	 * Inspect NAT policies in the ruleset for port map sharing.
	 * Note that npf_ruleset_sharepm() will increase the reference count.
	 */
	if (!npf_ruleset_sharepm(rset, np)) {
		/* Allocate a new port map for the NAT policy. */
		pm = kmem_zalloc(PORTMAP_MEM_SIZE, KM_SLEEP);
		pm->p_refcnt = 1;
		KASSERT((uintptr_t)pm->p_bitmap == (uintptr_t)pm + sizeof(*pm));
		np->n_portmap = pm;
	} else {
		KASSERT(np->n_portmap != NULL);
		KASSERT(np->n_portmap->p_refcnt > 0);
	}
	return np;
err:
	mutex_destroy(&np->n_lock);
	kmem_free(np, sizeof(npf_natpolicy_t));
	return NULL;
}

int
npf_nat_policyexport(const npf_natpolicy_t *np, prop_dictionary_t natdict)
{
	prop_data_t d;

	prop_dictionary_set_int32(natdict, "type", np->n_type);
	prop_dictionary_set_uint32(natdict, "flags", np->n_flags);

	d = prop_data_create_data(&np->n_taddr, np->n_alen);
	prop_dictionary_set_and_rel(natdict, "nat-ip", d);

	prop_dictionary_set_uint8(natdict, "nat-mask", np->n_tmask);
	prop_dictionary_set_uint16(natdict, "nat-port", np->n_tport);
	prop_dictionary_set_uint32(natdict, "nat-algo", np->n_algo);

	switch (np->n_algo) {
	case NPF_ALGO_NPT66:
		prop_dictionary_set_uint16(natdict, "npt66-adj", np->n_npt66_adj);
		break;
	}
	prop_dictionary_set_uint64(natdict, "nat-policy", np->n_id);
	return 0;
}

/*
 * npf_nat_freepolicy: free NAT policy and, on last reference, free portmap.
 *
 * => Called from npf_rule_free() during the reload via npf_ruleset_destroy().
 */
void
npf_nat_freepolicy(npf_natpolicy_t *np)
{
	npf_portmap_t *pm = np->n_portmap;
	npf_conn_t *con;
	npf_nat_t *nt;

	/*
	 * Disassociate all entries from the policy.  At this point,
	 * new entries can no longer be created for this policy.
	 */
	while (np->n_refcnt) {
		mutex_enter(&np->n_lock);
		LIST_FOREACH(nt, &np->n_nat_list, nt_entry) {
			con = nt->nt_conn;
			KASSERT(con != NULL);
			npf_conn_expire(con);
		}
		mutex_exit(&np->n_lock);

		/* Kick the worker - all references should be going away. */
		npf_worker_signal();
		kpause("npfgcnat", false, 1, NULL);
	}
	KASSERT(LIST_EMPTY(&np->n_nat_list));
	KASSERT(pm == NULL || pm->p_refcnt > 0);

	/* Destroy the port map, on last reference. */
	if (pm && atomic_dec_uint_nv(&pm->p_refcnt) == 0) {
		KASSERT((np->n_flags & NPF_NAT_PORTMAP) != 0);
		kmem_free(pm, PORTMAP_MEM_SIZE);
	}
	mutex_destroy(&np->n_lock);
	kmem_free(np, sizeof(npf_natpolicy_t));
}

void
npf_nat_freealg(npf_natpolicy_t *np, npf_alg_t *alg)
{
	npf_nat_t *nt;

	mutex_enter(&np->n_lock);
	LIST_FOREACH(nt, &np->n_nat_list, nt_entry) {
		if (nt->nt_alg == alg)
			nt->nt_alg = NULL;
	}
	mutex_exit(&np->n_lock);
}

/*
 * npf_nat_cmppolicy: compare two NAT policies.
 *
 * => Return 0 on match, and non-zero otherwise.
 */
bool
npf_nat_cmppolicy(npf_natpolicy_t *np, npf_natpolicy_t *mnp)
{
	const void *np_raw, *mnp_raw;

	/*
	 * Compare the relevant NAT policy information (in raw form),
	 * which is enough for matching criterion.
	 */
	KASSERT(np && mnp && np != mnp);
	np_raw = (const uint8_t *)np + NPF_NP_CMP_START;
	mnp_raw = (const uint8_t *)mnp + NPF_NP_CMP_START;
	return memcmp(np_raw, mnp_raw, NPF_NP_CMP_SIZE) == 0;
}

bool
npf_nat_sharepm(npf_natpolicy_t *np, npf_natpolicy_t *mnp)
{
	npf_portmap_t *pm, *mpm;

	KASSERT(np && mnp && np != mnp);
	KASSERT(LIST_EMPTY(&mnp->n_nat_list));
	KASSERT(mnp->n_refcnt == 0);

	/* Using port map and having equal translation address? */
	if ((np->n_flags & mnp->n_flags & NPF_NAT_PORTMAP) == 0) {
		return false;
	}
	if (np->n_alen != mnp->n_alen) {
		return false;
	}
	if (memcmp(&np->n_taddr, &mnp->n_taddr, np->n_alen) != 0) {
		return false;
	}
	mpm = mnp->n_portmap;
	KASSERT(mpm == NULL || mpm->p_refcnt > 0);

	/*
	 * If NAT policy has an old port map - drop the reference
	 * and destroy the port map if it was the last.
	 */
	if (mpm && atomic_dec_uint_nv(&mpm->p_refcnt) == 0) {
		kmem_free(mpm, PORTMAP_MEM_SIZE);
	}

	/* Share the port map. */
	pm = np->n_portmap;
	atomic_inc_uint(&pm->p_refcnt);
	mnp->n_portmap = pm;
	return true;
}

void
npf_nat_setid(npf_natpolicy_t *np, uint64_t id)
{
	np->n_id = id;
}

uint64_t
npf_nat_getid(const npf_natpolicy_t *np)
{
	return np->n_id;
}

/*
 * npf_nat_getport: allocate and return a port in the NAT policy portmap.
 *
 * => Returns in network byte-order.
 * => Zero indicates failure.
 */
static in_port_t
npf_nat_getport(npf_natpolicy_t *np)
{
	npf_portmap_t *pm = np->n_portmap;
	u_int n = PORTMAP_SIZE, idx, bit;
	uint32_t map, nmap;

	KASSERT((np->n_flags & NPF_NAT_PORTMAP) != 0);
	KASSERT(pm->p_refcnt > 0);

	idx = cprng_fast32() % PORTMAP_SIZE;
	for (;;) {
		KASSERT(idx < PORTMAP_SIZE);
		map = pm->p_bitmap[idx];
		if (__predict_false(map == PORTMAP_FILLED)) {
			if (n-- == 0) {
				/* No space. */
				return 0;
			}
			/* This bitmap is filled, next. */
			idx = (idx ? idx : PORTMAP_SIZE) - 1;
			continue;
		}
		bit = ffs32(~map) - 1;
		nmap = map | (1 << bit);
		if (atomic_cas_32(&pm->p_bitmap[idx], map, nmap) == map) {
			/* Success. */
			break;
		}
	}
	return htons(PORTMAP_FIRST + (idx << PORTMAP_SHIFT) + bit);
}

/*
 * npf_nat_takeport: allocate specific port in the NAT policy portmap.
 */
static bool
npf_nat_takeport(npf_natpolicy_t *np, in_port_t port)
{
	npf_portmap_t *pm = np->n_portmap;
	uint32_t map, nmap;
	u_int idx, bit;

	KASSERT((np->n_flags & NPF_NAT_PORTMAP) != 0);
	KASSERT(pm->p_refcnt > 0);

	port = ntohs(port) - PORTMAP_FIRST;
	idx = port >> PORTMAP_SHIFT;
	bit = port & PORTMAP_MASK;
	map = pm->p_bitmap[idx];
	nmap = map | (1 << bit);
	if (map == nmap) {
		/* Already taken. */
		return false;
	}
	return atomic_cas_32(&pm->p_bitmap[idx], map, nmap) == map;
}

/*
 * npf_nat_putport: return port as available in the NAT policy portmap.
 *
 * => Port should be in network byte-order.
 */
static void
npf_nat_putport(npf_natpolicy_t *np, in_port_t port)
{
	npf_portmap_t *pm = np->n_portmap;
	uint32_t map, nmap;
	u_int idx, bit;

	KASSERT((np->n_flags & NPF_NAT_PORTMAP) != 0);
	KASSERT(pm->p_refcnt > 0);

	port = ntohs(port) - PORTMAP_FIRST;
	idx = port >> PORTMAP_SHIFT;
	bit = port & PORTMAP_MASK;
	do {
		map = pm->p_bitmap[idx];
		KASSERT(map | (1 << bit));
		nmap = map & ~(1 << bit);
	} while (atomic_cas_32(&pm->p_bitmap[idx], map, nmap) != map);
}

/*
 * npf_nat_which: tell which address (source or destination) should be
 * rewritten given the combination of the NAT type and flow direction.
 */
static inline u_int
npf_nat_which(const int type, bool forw)
{
	/*
	 * Outbound NAT rewrites:
	 * - Source (NPF_SRC) on "forwards" stream.
	 * - Destination (NPF_DST) on "backwards" stream.
	 * Inbound NAT is other way round.
	 */
	if (type == NPF_NATOUT) {
		forw = !forw;
	} else {
		KASSERT(type == NPF_NATIN);
	}
	CTASSERT(NPF_SRC == 0 && NPF_DST == 1);
	KASSERT(forw == NPF_SRC || forw == NPF_DST);
	return (u_int)forw;
}

/*
 * npf_nat_inspect: inspect packet against NAT ruleset and return a policy.
 *
 * => Acquire a reference on the policy, if found.
 */
static npf_natpolicy_t *
npf_nat_inspect(npf_cache_t *npc, const int di)
{
	int slock = npf_config_read_enter();
	npf_ruleset_t *rlset = npf_config_natset();
	npf_natpolicy_t *np;
	npf_rule_t *rl;

	rl = npf_ruleset_inspect(npc, rlset, di, NPF_LAYER_3);
	if (rl == NULL) {
		npf_config_read_exit(slock);
		return NULL;
	}
	np = npf_rule_getnat(rl);
	atomic_inc_uint(&np->n_refcnt);
	npf_config_read_exit(slock);
	return np;
}

/*
 * npf_nat_create: create a new NAT translation entry.
 */
static npf_nat_t *
npf_nat_create(npf_cache_t *npc, npf_natpolicy_t *np, npf_conn_t *con)
{
	const int proto = npc->npc_proto;
	npf_nat_t *nt;

	KASSERT(npf_iscached(npc, NPC_IP46));
	KASSERT(npf_iscached(npc, NPC_LAYER4));

	/* Construct a new NAT entry and associate it with the connection. */
	nt = pool_cache_get(nat_cache, PR_NOWAIT);
	if (nt == NULL){
		return NULL;
	}
	npf_stats_inc(NPF_STAT_NAT_CREATE);
	nt->nt_natpolicy = np;
	nt->nt_conn = con;
	nt->nt_alg = NULL;

	/* Save the original address which may be rewritten. */
	if (np->n_type == NPF_NATOUT) {
		/* Outbound NAT: source (think internal) address. */
		memcpy(&nt->nt_oaddr, npc->npc_ips[NPF_SRC], npc->npc_alen);
	} else {
		/* Inbound NAT: destination (think external) address. */
		KASSERT(np->n_type == NPF_NATIN);
		memcpy(&nt->nt_oaddr, npc->npc_ips[NPF_DST], npc->npc_alen);
	}

	/*
	 * Port translation, if required, and if it is TCP/UDP.
	 */
	if ((np->n_flags & NPF_NAT_PORTS) == 0 ||
	    (proto != IPPROTO_TCP && proto != IPPROTO_UDP)) {
		nt->nt_oport = 0;
		nt->nt_tport = 0;
		goto out;
	}

	/* Save the relevant TCP/UDP port. */
	if (proto == IPPROTO_TCP) {
		const struct tcphdr *th = npc->npc_l4.tcp;
		nt->nt_oport = (np->n_type == NPF_NATOUT) ?
		    th->th_sport : th->th_dport;
	} else {
		const struct udphdr *uh = npc->npc_l4.udp;
		nt->nt_oport = (np->n_type == NPF_NATOUT) ?
		    uh->uh_sport : uh->uh_dport;
	}

	/* Get a new port for translation. */
	if ((np->n_flags & NPF_NAT_PORTMAP) != 0) {
		nt->nt_tport = npf_nat_getport(np);
	} else {
		nt->nt_tport = np->n_tport;
	}
out:
	mutex_enter(&np->n_lock);
	LIST_INSERT_HEAD(&np->n_nat_list, nt, nt_entry);
	mutex_exit(&np->n_lock);
	return nt;
}

/*
 * npf_nat_translate: perform translation given the state data.
 */
static inline int
npf_nat_translate(npf_cache_t *npc, npf_nat_t *nt, bool forw)
{
	const npf_natpolicy_t *np = nt->nt_natpolicy;
	const u_int which = npf_nat_which(np->n_type, forw);
	const npf_addr_t *addr;
	in_port_t port;

	KASSERT(npf_iscached(npc, NPC_IP46));
	KASSERT(npf_iscached(npc, NPC_LAYER4));

	if (forw) {
		/* "Forwards" stream: use translation address/port. */
		addr = &np->n_taddr;
		port = nt->nt_tport;
	} else {
		/* "Backwards" stream: use original address/port. */
		addr = &nt->nt_oaddr;
		port = nt->nt_oport;
	}
	KASSERT((np->n_flags & NPF_NAT_PORTS) != 0 || port == 0);

	/* Execute ALG translation first. */
	if ((npc->npc_info & NPC_ALG_EXEC) == 0) {
		npc->npc_info |= NPC_ALG_EXEC;
		npf_alg_exec(npc, nt, forw);
		npf_recache(npc);
	}
	KASSERT(!nbuf_flag_p(npc->npc_nbuf, NBUF_DATAREF_RESET));

	/* Finally, perform the translation. */
	return npf_napt_rwr(npc, which, addr, port);
}

/*
 * npf_nat_algo: perform the translation given the algorithm.
 */
static inline int
npf_nat_algo(npf_cache_t *npc, const npf_natpolicy_t *np, bool forw)
{
	const u_int which = npf_nat_which(np->n_type, forw);
	int error;

	switch (np->n_algo) {
	case NPF_ALGO_NPT66:
		error = npf_npt66_rwr(npc, which, &np->n_taddr,
		    np->n_tmask, np->n_npt66_adj);
		break;
	default:
		error = npf_napt_rwr(npc, which, &np->n_taddr, np->n_tport);
		break;
	}

	return error;
}

/*
 * npf_do_nat:
 *	- Inspect packet for a NAT policy, unless a connection with a NAT
 *	  association already exists.  In such case, determine whether it
 *	  is a "forwards" or "backwards" stream.
 *	- Perform translation: rewrite source or destination fields,
 *	  depending on translation type and direction.
 *	- Associate a NAT policy with a connection (may establish a new).
 */
int
npf_do_nat(npf_cache_t *npc, npf_conn_t *con, const int di)
{
	nbuf_t *nbuf = npc->npc_nbuf;
	npf_conn_t *ncon = NULL;
	npf_natpolicy_t *np;
	npf_nat_t *nt;
	int error;
	bool forw;

	/* All relevant IPv4 data should be already cached. */
	if (!npf_iscached(npc, NPC_IP46) || !npf_iscached(npc, NPC_LAYER4)) {
		return 0;
	}
	KASSERT(!nbuf_flag_p(nbuf, NBUF_DATAREF_RESET));

	/*
	 * Return the NAT entry associated with the connection, if any.
	 * Determines whether the stream is "forwards" or "backwards".
	 * Note: no need to lock, since reference on connection is held.
	 */
	if (con && (nt = npf_conn_getnat(con, di, &forw)) != NULL) {
		np = nt->nt_natpolicy;
		goto translate;
	}

	/*
	 * Inspect the packet for a NAT policy, if there is no connection.
	 * Note: acquires a reference if found.
	 */
	np = npf_nat_inspect(npc, di);
	if (np == NULL) {
		/* If packet does not match - done. */
		return 0;
	}
	forw = true;

	/* Static NAT - just perform the translation. */
	if (np->n_flags & NPF_NAT_STATIC) {
		if (nbuf_cksum_barrier(nbuf, di)) {
			npf_recache(npc);
		}
		error = npf_nat_algo(npc, np, forw);
		atomic_dec_uint(&np->n_refcnt);
		return error;
	}

	/*
	 * If there is no local connection (no "stateful" rule - unusual,
	 * but possible configuration), establish one before translation.
	 * Note that it is not a "pass" connection, therefore passing of
	 * "backwards" stream depends on other, stateless filtering rules.
	 */
	if (con == NULL) {
		ncon = npf_conn_establish(npc, di, true);
		if (ncon == NULL) {
			atomic_dec_uint(&np->n_refcnt);
			return ENOMEM;
		}
		con = ncon;
	}

	/*
	 * Create a new NAT entry and associate with the connection.
	 * We will consume the reference on success (release on error).
	 */
	nt = npf_nat_create(npc, np, con);
	if (nt == NULL) {
		atomic_dec_uint(&np->n_refcnt);
		error = ENOMEM;
		goto out;
	}

	/* Associate the NAT translation entry with the connection. */
	error = npf_conn_setnat(npc, con, nt, np->n_type);
	if (error) {
		/* Will release the reference. */
		npf_nat_destroy(nt);
		goto out;
	}

	/* Determine whether any ALG matches. */
	if (npf_alg_match(npc, nt, di)) {
		KASSERT(nt->nt_alg != NULL);
	}

translate:
	/* May need to process the delayed checksums first (XXX: NetBSD). */
	if (nbuf_cksum_barrier(nbuf, di)) {
		npf_recache(npc);
	}

	/* Perform the translation. */
	error = npf_nat_translate(npc, nt, forw);
out:
	if (__predict_false(ncon)) {
		if (error) {
			/* It created for NAT - just expire. */
			npf_conn_expire(ncon);
		}
		npf_conn_release(ncon);
	}
	return error;
}

/*
 * npf_nat_gettrans: return translation IP address and port.
 */
void
npf_nat_gettrans(npf_nat_t *nt, npf_addr_t **addr, in_port_t *port)
{
	npf_natpolicy_t *np = nt->nt_natpolicy;

	*addr = &np->n_taddr;
	*port = nt->nt_tport;
}

/*
 * npf_nat_getorig: return original IP address and port from translation entry.
 */
void
npf_nat_getorig(npf_nat_t *nt, npf_addr_t **addr, in_port_t *port)
{
	*addr = &nt->nt_oaddr;
	*port = nt->nt_oport;
}

/*
 * npf_nat_setalg: associate an ALG with the NAT entry.
 */
void
npf_nat_setalg(npf_nat_t *nt, npf_alg_t *alg, uintptr_t arg)
{
	nt->nt_alg = alg;
	nt->nt_alg_arg = arg;
}

/*
 * npf_nat_destroy: destroy NAT structure (performed on connection expiration).
 */
void
npf_nat_destroy(npf_nat_t *nt)
{
	npf_natpolicy_t *np = nt->nt_natpolicy;

	/* Return any taken port to the portmap. */
	if ((np->n_flags & NPF_NAT_PORTMAP) != 0 && nt->nt_tport) {
		npf_nat_putport(np, nt->nt_tport);
	}

	mutex_enter(&np->n_lock);
	LIST_REMOVE(nt, nt_entry);
	KASSERT(np->n_refcnt > 0);
	atomic_dec_uint(&np->n_refcnt);
	mutex_exit(&np->n_lock);

	pool_cache_put(nat_cache, nt);
	npf_stats_inc(NPF_STAT_NAT_DESTROY);
}

/*
 * npf_nat_export: serialise the NAT entry with a NAT policy ID.
 */
void
npf_nat_export(prop_dictionary_t condict, npf_nat_t *nt)
{
	npf_natpolicy_t *np = nt->nt_natpolicy;
	prop_dictionary_t natdict;
	prop_data_t d;

	natdict = prop_dictionary_create();
	d = prop_data_create_data(&nt->nt_oaddr, sizeof(npf_addr_t));
	prop_dictionary_set_and_rel(natdict, "oaddr", d);
	prop_dictionary_set_uint16(natdict, "oport", nt->nt_oport);
	prop_dictionary_set_uint16(natdict, "tport", nt->nt_tport);
	prop_dictionary_set_uint64(natdict, "nat-policy", np->n_id);
	prop_dictionary_set_and_rel(condict, "nat", natdict);
}

/*
 * npf_nat_import: find the NAT policy and unserialise the NAT entry.
 */
npf_nat_t *
npf_nat_import(prop_dictionary_t natdict, npf_ruleset_t *natlist,
    npf_conn_t *con)
{
	npf_natpolicy_t *np;
	npf_nat_t *nt;
	uint64_t np_id;
	const void *d;

	prop_dictionary_get_uint64(natdict, "nat-policy", &np_id);
	if ((np = npf_ruleset_findnat(natlist, np_id)) == NULL) {
		return NULL;
	}
	nt = pool_cache_get(nat_cache, PR_WAITOK);
	memset(nt, 0, sizeof(npf_nat_t));

	prop_object_t obj = prop_dictionary_get(natdict, "oaddr");
	if ((d = prop_data_data_nocopy(obj)) == NULL ||
	    prop_data_size(obj) != sizeof(npf_addr_t)) {
		pool_cache_put(nat_cache, nt);
		return NULL;
	}
	memcpy(&nt->nt_oaddr, d, sizeof(npf_addr_t));
	prop_dictionary_get_uint16(natdict, "oport", &nt->nt_oport);
	prop_dictionary_get_uint16(natdict, "tport", &nt->nt_tport);

	/* Take a specific port from port-map. */
	if ((np->n_flags & NPF_NAT_PORTMAP) != 0 && nt->nt_tport &
	    !npf_nat_takeport(np, nt->nt_tport)) {
		pool_cache_put(nat_cache, nt);
		return NULL;
	}
	npf_stats_inc(NPF_STAT_NAT_CREATE);

	/*
	 * Associate, take a reference and insert.  Unlocked since
	 * the policy is not yet visible.
	 */
	nt->nt_natpolicy = np;
	nt->nt_conn = con;
	np->n_refcnt++;
	LIST_INSERT_HEAD(&np->n_nat_list, nt, nt_entry);
	return nt;
}

#if defined(DDB) || defined(_NPF_TESTING)

void
npf_nat_dump(const npf_nat_t *nt)
{
	const npf_natpolicy_t *np;
	struct in_addr ip;

	np = nt->nt_natpolicy;
	memcpy(&ip, &np->n_taddr, sizeof(ip));
	printf("\tNATP(%p): type %d flags 0x%x taddr %s tport %d\n", np,
	    np->n_type, np->n_flags, inet_ntoa(ip), ntohs(np->n_tport));
	memcpy(&ip, &nt->nt_oaddr, sizeof(ip));
	printf("\tNAT: original address %s oport %d tport %d\n",
	    inet_ntoa(ip), ntohs(nt->nt_oport), ntohs(nt->nt_tport));
	if (nt->nt_alg) {
		printf("\tNAT ALG = %p, ARG = %p\n",
		    nt->nt_alg, (void *)nt->nt_alg_arg);
	}
}

#endif
