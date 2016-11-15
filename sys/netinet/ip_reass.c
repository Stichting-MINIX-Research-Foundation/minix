/*	$NetBSD: ip_reass.c,v 1.9 2014/02/25 18:30:12 pooka Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

/*
 * IP reassembly.
 *
 * Additive-Increase/Multiplicative-Decrease (AIMD) strategy for IP
 * reassembly queue buffer managment.
 *
 * We keep a count of total IP fragments (NB: not fragmented packets),
 * awaiting reassembly (ip_nfrags) and a limit (ip_maxfrags) on fragments.
 * If ip_nfrags exceeds ip_maxfrags the limit, we drop half the total
 * fragments in reassembly queues.  This AIMD policy avoids repeatedly
 * deleting single packets under heavy fragmentation load (e.g., from lossy
 * NFS peers).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_reass.c,v 1.9 2014/02/25 18:30:12 pooka Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/in_proto.h>
#include <netinet/ip_private.h>
#include <netinet/in_var.h>

/*
 * IP reassembly queue structures.  Each fragment being reassembled is
 * attached to one of these structures.  They are timed out after TTL
 * drops to 0, and may also be reclaimed if memory becomes tight.
 */

typedef struct ipfr_qent {
	TAILQ_ENTRY(ipfr_qent)	ipqe_q;
	struct ip *		ipqe_ip;
	struct mbuf *		ipqe_m;
	bool			ipqe_mff;
} ipfr_qent_t;

TAILQ_HEAD(ipfr_qent_head, ipfr_qent);

typedef struct ipfr_queue {
	LIST_ENTRY(ipfr_queue)	ipq_q;		/* to other reass headers */
	struct ipfr_qent_head	ipq_fragq;	/* queue of fragment entries */
	uint8_t			ipq_ttl;	/* time for reass q to live */
	uint8_t			ipq_p;		/* protocol of this fragment */
	uint16_t		ipq_id;		/* sequence id for reassembly */
	struct in_addr		ipq_src;
	struct in_addr		ipq_dst;
	uint16_t		ipq_nfrags;	/* frags in this queue entry */
	uint8_t 		ipq_tos;	/* TOS of this fragment */
} ipfr_queue_t;

/*
 * Hash table of IP reassembly queues.
 */
#define	IPREASS_HASH_SHIFT	6
#define	IPREASS_HASH_SIZE	(1 << IPREASS_HASH_SHIFT)
#define	IPREASS_HASH_MASK	(IPREASS_HASH_SIZE - 1)
#define	IPREASS_HASH(x, y) \
	(((((x) & 0xf) | ((((x) >> 8) & 0xf) << 4)) ^ (y)) & IPREASS_HASH_MASK)

static LIST_HEAD(, ipfr_queue)	ip_frags[IPREASS_HASH_SIZE];
static pool_cache_t	ipfren_cache;
static kmutex_t		ipfr_lock;

/* Number of packets in reassembly queue and total number of fragments. */
static int		ip_nfragpackets;
static int		ip_nfrags;

/* Limits on packet and fragments. */
static int		ip_maxfragpackets;
static int		ip_maxfrags;

/*
 * Cached copy of nmbclusters.  If nbclusters is different, recalculate
 * IP parameters derived from nmbclusters.
 */
static int		ip_nmbclusters;

/*
 * IP reassembly TTL machinery for multiplicative drop.
 */
static u_int		fragttl_histo[IPFRAGTTL + 1];

static struct sysctllog *ip_reass_sysctllog;

void			sysctl_ip_reass_setup(void);
static void		ip_nmbclusters_changed(void);

static struct mbuf *	ip_reass(ipfr_qent_t *, ipfr_queue_t *, u_int);
static u_int		ip_reass_ttl_decr(u_int ticks);
static void		ip_reass_drophalf(void);
static void		ip_freef(ipfr_queue_t *);

/*
 * ip_reass_init:
 *
 *	Initialization of IP reassembly mechanism.
 */
void
ip_reass_init(void)
{
	int i;

	ipfren_cache = pool_cache_init(sizeof(ipfr_qent_t), coherency_unit,
	    0, 0, "ipfrenpl", NULL, IPL_NET, NULL, NULL, NULL);
	mutex_init(&ipfr_lock, MUTEX_DEFAULT, IPL_VM);

	for (i = 0; i < IPREASS_HASH_SIZE; i++) {
		LIST_INIT(&ip_frags[i]);
	}
	ip_maxfragpackets = 200;
	ip_maxfrags = 0;
	ip_nmbclusters_changed();

	sysctl_ip_reass_setup();
}

void
sysctl_ip_reass_setup(void)
{

	sysctl_createv(&ip_reass_sysctllog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "inet",
		SYSCTL_DESCR("PF_INET related settings"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_INET, CTL_EOL);
	sysctl_createv(&ip_reass_sysctllog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "ip",
		SYSCTL_DESCR("IPv4 related settings"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_INET, IPPROTO_IP, CTL_EOL);

	sysctl_createv(&ip_reass_sysctllog, 0, NULL, NULL,
		CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		CTLTYPE_INT, "maxfragpackets",
		SYSCTL_DESCR("Maximum number of fragments to retain for "
			     "possible reassembly"),
		NULL, 0, &ip_maxfragpackets, 0,
		CTL_NET, PF_INET, IPPROTO_IP, IPCTL_MAXFRAGPACKETS, CTL_EOL);
}

#define CHECK_NMBCLUSTER_PARAMS()				\
do {								\
	if (__predict_false(ip_nmbclusters != nmbclusters))	\
		ip_nmbclusters_changed();			\
} while (/*CONSTCOND*/0)

/*
 * Compute IP limits derived from the value of nmbclusters.
 */
static void
ip_nmbclusters_changed(void)
{
	ip_maxfrags = nmbclusters / 4;
	ip_nmbclusters = nmbclusters;
}

/*
 * ip_reass:
 *
 *	Take incoming datagram fragment and try to reassemble it into whole
 *	datagram.  If a chain for reassembly of this datagram already exists,
 *	then it is given as 'fp'; otherwise have to make a chain.
 */
struct mbuf *
ip_reass(ipfr_qent_t *ipqe, ipfr_queue_t *fp, const u_int hash)
{
	struct ip *ip = ipqe->ipqe_ip, *qip;
	const int hlen = ip->ip_hl << 2;
	struct mbuf *m = ipqe->ipqe_m, *t;
	ipfr_qent_t *nq, *p, *q;
	int i, next;

	KASSERT(mutex_owned(&ipfr_lock));

	/*
	 * Presence of header sizes in mbufs would confuse code below.
	 */
	m->m_data += hlen;
	m->m_len -= hlen;

#ifdef	notyet
	/* Make sure fragment limit is up-to-date. */
	CHECK_NMBCLUSTER_PARAMS();

	/* If we have too many fragments, drop the older half. */
	if (ip_nfrags >= ip_maxfrags) {
		ip_reass_drophalf(void);
	}
#endif

	/*
	 * We are about to add a fragment; increment frag count.
	 */
	ip_nfrags++;

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == NULL) {
		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly:  a) if maxfrag is 0,
		 * never accept fragments  b) if maxfrag is -1, accept
		 * all fragments without limitation.
		 */
		if (ip_maxfragpackets < 0)
			;
		else if (ip_nfragpackets >= ip_maxfragpackets) {
			goto dropfrag;
		}
		fp = malloc(sizeof(ipfr_queue_t), M_FTABLE, M_NOWAIT);
		if (fp == NULL) {
			goto dropfrag;
		}
		ip_nfragpackets++;
		TAILQ_INIT(&fp->ipq_fragq);
		fp->ipq_nfrags = 1;
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ip->ip_p;
		fp->ipq_id = ip->ip_id;
		fp->ipq_tos = ip->ip_tos;
		fp->ipq_src = ip->ip_src;
		fp->ipq_dst = ip->ip_dst;
		LIST_INSERT_HEAD(&ip_frags[hash], fp, ipq_q);
		p = NULL;
		goto insert;
	} else {
		fp->ipq_nfrags++;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	TAILQ_FOREACH(q, &fp->ipq_fragq, ipqe_q) {
		if (ntohs(q->ipqe_ip->ip_off) > ntohs(ip->ip_off))
			break;
	}
	if (q != NULL) {
		p = TAILQ_PREV(q, ipfr_qent_head, ipqe_q);
	} else {
		p = TAILQ_LAST(&fp->ipq_fragq, ipfr_qent_head);
	}

	/*
	 * If there is a preceding segment, it may provide some of our
	 * data already.  If so, drop the data from the incoming segment.
	 * If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		i = ntohs(p->ipqe_ip->ip_off) + ntohs(p->ipqe_ip->ip_len) -
		    ntohs(ip->ip_off);
		if (i > 0) {
			if (i >= ntohs(ip->ip_len)) {
				goto dropfrag;
			}
			m_adj(ipqe->ipqe_m, i);
			ip->ip_off = htons(ntohs(ip->ip_off) + i);
			ip->ip_len = htons(ntohs(ip->ip_len) - i);
		}
	}

	/*
	 * While we overlap succeeding segments trim them or, if they are
	 * completely covered, dequeue them.
	 */
	while (q != NULL) {
		size_t end;

		qip = q->ipqe_ip;
		end = ntohs(ip->ip_off) + ntohs(ip->ip_len);
		if (end <= ntohs(qip->ip_off)) {
			break;
		}
		i = end - ntohs(qip->ip_off);
		if (i < ntohs(qip->ip_len)) {
			qip->ip_len = htons(ntohs(qip->ip_len) - i);
			qip->ip_off = htons(ntohs(qip->ip_off) + i);
			m_adj(q->ipqe_m, i);
			break;
		}
		nq = TAILQ_NEXT(q, ipqe_q);
		m_freem(q->ipqe_m);
		TAILQ_REMOVE(&fp->ipq_fragq, q, ipqe_q);
		pool_cache_put(ipfren_cache, q);
		fp->ipq_nfrags--;
		ip_nfrags--;
		q = nq;
	}

insert:
	/*
	 * Stick new segment in its place; check for complete reassembly.
	 */
	if (p == NULL) {
		TAILQ_INSERT_HEAD(&fp->ipq_fragq, ipqe, ipqe_q);
	} else {
		TAILQ_INSERT_AFTER(&fp->ipq_fragq, p, ipqe, ipqe_q);
	}
	next = 0;
	TAILQ_FOREACH(q, &fp->ipq_fragq, ipqe_q) {
		qip = q->ipqe_ip;
		if (ntohs(qip->ip_off) != next) {
			mutex_exit(&ipfr_lock);
			return NULL;
		}
		next += ntohs(qip->ip_len);
	}
	p = TAILQ_LAST(&fp->ipq_fragq, ipfr_qent_head);
	if (p->ipqe_mff) {
		mutex_exit(&ipfr_lock);
		return NULL;
	}

	/*
	 * Reassembly is complete.  Check for a bogus message size.
	 */
	q = TAILQ_FIRST(&fp->ipq_fragq);
	ip = q->ipqe_ip;
	if ((next + (ip->ip_hl << 2)) > IP_MAXPACKET) {
		IP_STATINC(IP_STAT_TOOLONG);
		ip_freef(fp);
		mutex_exit(&ipfr_lock);
		return NULL;
	}
	LIST_REMOVE(fp, ipq_q);
	ip_nfrags -= fp->ipq_nfrags;
	ip_nfragpackets--;
	mutex_exit(&ipfr_lock);

	/* Concatenate all fragments. */
	m = q->ipqe_m;
	t = m->m_next;
	m->m_next = NULL;
	m_cat(m, t);
	nq = TAILQ_NEXT(q, ipqe_q);
	pool_cache_put(ipfren_cache, q);

	for (q = nq; q != NULL; q = nq) {
		t = q->ipqe_m;
		nq = TAILQ_NEXT(q, ipqe_q);
		pool_cache_put(ipfren_cache, q);
		m_cat(m, t);
	}

	/*
	 * Create header for new packet by modifying header of first
	 * packet.  Dequeue and discard fragment reassembly header.  Make
	 * header visible.
	 */
	ip->ip_len = htons((ip->ip_hl << 2) + next);
	ip->ip_src = fp->ipq_src;
	ip->ip_dst = fp->ipq_dst;
	free(fp, M_FTABLE);

	m->m_len += (ip->ip_hl << 2);
	m->m_data -= (ip->ip_hl << 2);

	/* Fix up mbuf.  XXX This should be done elsewhere. */
	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (t = m; t; t = t->m_next) {
			plen += t->m_len;
		}
		m->m_pkthdr.len = plen;
		m->m_pkthdr.csum_flags = 0;
	}
	return m;

dropfrag:
	if (fp != NULL) {
		fp->ipq_nfrags--;
	}
	ip_nfrags--;
	IP_STATINC(IP_STAT_FRAGDROPPED);
	mutex_exit(&ipfr_lock);

	pool_cache_put(ipfren_cache, ipqe);
	m_freem(m);
	return NULL;
}

/*
 * ip_freef:
 *
 *	Free a fragment reassembly header and all associated datagrams.
 */
static void
ip_freef(ipfr_queue_t *fp)
{
	ipfr_qent_t *q;

	KASSERT(mutex_owned(&ipfr_lock));

	LIST_REMOVE(fp, ipq_q);
	ip_nfrags -= fp->ipq_nfrags;
	ip_nfragpackets--;

	while ((q = TAILQ_FIRST(&fp->ipq_fragq)) != NULL) {
		TAILQ_REMOVE(&fp->ipq_fragq, q, ipqe_q);
		m_freem(q->ipqe_m);
		pool_cache_put(ipfren_cache, q);
	}
	free(fp, M_FTABLE);
}

/*
 * ip_reass_ttl_decr:
 *
 *	Decrement TTL of all reasembly queue entries by `ticks'.  Count
 *	number of distinct fragments (as opposed to partial, fragmented
 *	datagrams) inthe reassembly queue.  While we  traverse the entire
 *	reassembly queue, compute and return the median TTL over all
 *	fragments.
 */
static u_int
ip_reass_ttl_decr(u_int ticks)
{
	u_int nfrags, median, dropfraction, keepfraction;
	ipfr_queue_t *fp, *nfp;
	int i;

	nfrags = 0;
	memset(fragttl_histo, 0, sizeof(fragttl_histo));

	for (i = 0; i < IPREASS_HASH_SIZE; i++) {
		for (fp = LIST_FIRST(&ip_frags[i]); fp != NULL; fp = nfp) {
			fp->ipq_ttl = ((fp->ipq_ttl <= ticks) ?
			    0 : fp->ipq_ttl - ticks);
			nfp = LIST_NEXT(fp, ipq_q);
			if (fp->ipq_ttl == 0) {
				IP_STATINC(IP_STAT_FRAGTIMEOUT);
				ip_freef(fp);
			} else {
				nfrags += fp->ipq_nfrags;
				fragttl_histo[fp->ipq_ttl] += fp->ipq_nfrags;
			}
		}
	}

	KASSERT(ip_nfrags == nfrags);

	/* Find median (or other drop fraction) in histogram. */
	dropfraction = (ip_nfrags / 2);
	keepfraction = ip_nfrags - dropfraction;
	for (i = IPFRAGTTL, median = 0; i >= 0; i--) {
		median += fragttl_histo[i];
		if (median >= keepfraction)
			break;
	}

	/* Return TTL of median (or other fraction). */
	return (u_int)i;
}

static void
ip_reass_drophalf(void)
{
	u_int median_ticks;

	KASSERT(mutex_owned(&ipfr_lock));

	/*
	 * Compute median TTL of all fragments, and count frags
	 * with that TTL or lower (roughly half of all fragments).
	 */
	median_ticks = ip_reass_ttl_decr(0);

	/* Drop half. */
	median_ticks = ip_reass_ttl_decr(median_ticks);
}

/*
 * ip_reass_drain: drain off all datagram fragments.  Do not acquire
 * softnet_lock as can be called from hardware interrupt context.
 */
void
ip_reass_drain(void)
{

	/*
	 * We may be called from a device's interrupt context.  If
	 * the ipq is already busy, just bail out now.
	 */
	if (mutex_tryenter(&ipfr_lock)) {
		/*
		 * Drop half the total fragments now. If more mbufs are
		 * needed, we will be called again soon.
		 */
		ip_reass_drophalf();
		mutex_exit(&ipfr_lock);
	}
}

/*
 * ip_reass_slowtimo:
 *
 *	If a timer expires on a reassembly queue, discard it.
 */
void
ip_reass_slowtimo(void)
{
	static u_int dropscanidx = 0;
	u_int i, median_ttl;

	mutex_enter(&ipfr_lock);

	/* Age TTL of all fragments by 1 tick .*/
	median_ttl = ip_reass_ttl_decr(1);

	/* Make sure fragment limit is up-to-date. */
	CHECK_NMBCLUSTER_PARAMS();

	/* If we have too many fragments, drop the older half. */
	if (ip_nfrags > ip_maxfrags) {
		ip_reass_ttl_decr(median_ttl);
	}

	/*
	 * If we are over the maximum number of fragmented packets (due to
	 * the limit being lowered), drain off enough to get down to the
	 * new limit.  Start draining from the reassembly hashqueue most
	 * recently drained.
	 */
	if (ip_maxfragpackets < 0)
		;
	else {
		int wrapped = 0;

		i = dropscanidx;
		while (ip_nfragpackets > ip_maxfragpackets && wrapped == 0) {
			while (LIST_FIRST(&ip_frags[i]) != NULL) {
				ip_freef(LIST_FIRST(&ip_frags[i]));
			}
			if (++i >= IPREASS_HASH_SIZE) {
				i = 0;
			}
			/*
			 * Do not scan forever even if fragment counters are
			 * wrong: stop after scanning entire reassembly queue.
			 */
			if (i == dropscanidx) {
				wrapped = 1;
			}
		}
		dropscanidx = i;
	}
	mutex_exit(&ipfr_lock);
}

/*
 * ip_reass_packet: generic routine to perform IP reassembly.
 *
 * => Passed fragment should have IP_MF flag and/or offset set.
 * => Fragment should not have other than IP_MF flags set.
 *
 * => Returns 0 on success or error otherwise.
 * => On complete, m0 represents a constructed final packet.
 */
int
ip_reass_packet(struct mbuf **m0, struct ip *ip)
{
	const int hlen = ip->ip_hl << 2;
	const int len = ntohs(ip->ip_len);
	struct mbuf *m = *m0;
	ipfr_queue_t *fp;
	ipfr_qent_t *ipqe;
	u_int hash, off, flen;
	bool mff;

	/*
	 * Prevent TCP blind data attacks by not allowing non-initial
	 * fragments to start at less than 68 bytes (minimal fragment
	 * size) and making sure the first fragment is at least 68
	 * bytes.
	 */
	off = (ntohs(ip->ip_off) & IP_OFFMASK) << 3;
	if ((off > 0 ? off + hlen : len) < IP_MINFRAGSIZE - 1) {
		IP_STATINC(IP_STAT_BADFRAGS);
		return EINVAL;
	}

	/*
	 * Fragment length and MF flag.  Make sure that fragments have
	 * a data length which is non-zero and multiple of 8 bytes.
	 */
	flen = ntohs(ip->ip_len) - hlen;
	mff = (ip->ip_off & htons(IP_MF)) != 0;
	if (mff && (flen == 0 || (flen & 0x7) != 0)) {
		IP_STATINC(IP_STAT_BADFRAGS);
		return EINVAL;
	}

	/*
	 * Adjust total IP length to not reflect header and convert
	 * offset of this to bytes.  XXX: clobbers struct ip.
	 */
	ip->ip_len = htons(flen);
	ip->ip_off = htons(off);

	/* Look for queue of fragments of this datagram. */
	mutex_enter(&ipfr_lock);
	hash = IPREASS_HASH(ip->ip_src.s_addr, ip->ip_id);
	LIST_FOREACH(fp, &ip_frags[hash], ipq_q) {
		if (ip->ip_id != fp->ipq_id)
			continue;
		if (!in_hosteq(ip->ip_src, fp->ipq_src))
			continue;
		if (!in_hosteq(ip->ip_dst, fp->ipq_dst))
			continue;
		if (ip->ip_p != fp->ipq_p)
			continue;
		break;
	}

	/* Make sure that TOS matches previous fragments. */
	if (fp && fp->ipq_tos != ip->ip_tos) {
		IP_STATINC(IP_STAT_BADFRAGS);
		mutex_exit(&ipfr_lock);
		return EINVAL;
	}

	/*
	 * Create new entry and attempt to reassembly.
	 */
	IP_STATINC(IP_STAT_FRAGMENTS);
	ipqe = pool_cache_get(ipfren_cache, PR_NOWAIT);
	if (ipqe == NULL) {
		IP_STATINC(IP_STAT_RCVMEMDROP);
		mutex_exit(&ipfr_lock);
		return ENOMEM;
	}
	ipqe->ipqe_mff = mff;
	ipqe->ipqe_m = m;
	ipqe->ipqe_ip = ip;

	*m0 = ip_reass(ipqe, fp, hash);
	if (*m0) {
		/* Note that finally reassembled. */
		IP_STATINC(IP_STAT_REASSEMBLED);
	}
	return 0;
}
