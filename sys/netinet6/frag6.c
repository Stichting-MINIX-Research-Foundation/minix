/*	$NetBSD: frag6.c,v 1.56 2014/09/05 05:33:06 matt Exp $	*/
/*	$KAME: frag6.c,v 1.40 2002/05/27 21:40:31 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: frag6.c,v 1.56 2014/09/05 05:33:06 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_private.h>
#include <netinet/icmp6.h>

#include <net/net_osdep.h>

static void frag6_enq(struct ip6asfrag *, struct ip6asfrag *);
static void frag6_deq(struct ip6asfrag *);
static void frag6_insque(struct ip6q *, struct ip6q *);
static void frag6_remque(struct ip6q *);
static void frag6_freef(struct ip6q *);

static int frag6_drainwanted;

u_int frag6_nfragpackets;
u_int frag6_nfrags;
struct	ip6q ip6q;	/* ip6 reassemble queue */

static kmutex_t	frag6_lock;

/*
 * Initialise reassembly queue and fragment identifier.
 */
void
frag6_init(void)
{

	ip6q.ip6q_next = ip6q.ip6q_prev = &ip6q;
	mutex_init(&frag6_lock, MUTEX_DEFAULT, IPL_NET);
}

/*
 * IPv6 fragment input.
 *
 * In RFC2460, fragment and reassembly rule do not agree with each other,
 * in terms of next header field handling in fragment header.
 * While the sender will use the same value for all of the fragmented packets,
 * receiver is suggested not to check the consistency.
 *
 * fragment rule (p20):
 *	(2) A Fragment header containing:
 *	The Next Header value that identifies the first header of
 *	the Fragmentable Part of the original packet.
 *		-> next header field is same for all fragments
 *
 * reassembly rule (p21):
 *	The Next Header field of the last header of the Unfragmentable
 *	Part is obtained from the Next Header field of the first
 *	fragment's Fragment header.
 *		-> should grab it from the first fragment only
 *
 * The following note also contradicts with fragment rule - noone is going to
 * send different fragment with different next header field.
 *
 * additional note (p22):
 *	The Next Header values in the Fragment headers of different
 *	fragments of the same original packet may differ.  Only the value
 *	from the Offset zero fragment packet is used for reassembly.
 *		-> should grab it from the first fragment only
 *
 * There is no explicit reason given in the RFC.  Historical reason maybe?
 */
int
frag6_input(struct mbuf **mp, int *offp, int proto)
{
	struct rtentry *rt;
	struct mbuf *m = *mp, *t;
	struct ip6_hdr *ip6;
	struct ip6_frag *ip6f;
	struct ip6q *q6;
	struct ip6asfrag *af6, *ip6af, *af6dwn;
	int offset = *offp, nxt, i, next;
	int first_frag = 0;
	int fragoff, frgpartlen;	/* must be larger than u_int16_t */
	struct ifnet *dstifp;
	static struct route ro;
	union {
		struct sockaddr		dst;
		struct sockaddr_in6	dst6;
	} u;

	ip6 = mtod(m, struct ip6_hdr *);
	IP6_EXTHDR_GET(ip6f, struct ip6_frag *, m, offset, sizeof(*ip6f));
	if (ip6f == NULL)
		return IPPROTO_DONE;

	dstifp = NULL;
	/* find the destination interface of the packet. */
	sockaddr_in6_init(&u.dst6, &ip6->ip6_dst, 0, 0, 0);
	if ((rt = rtcache_lookup(&ro, &u.dst)) != NULL && rt->rt_ifa != NULL)
		dstifp = ((struct in6_ifaddr *)rt->rt_ifa)->ia_ifp;

	/* jumbo payload can't contain a fragment header */
	if (ip6->ip6_plen == 0) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER, offset);
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	/*
	 * check whether fragment packet's fragment length is
	 * multiple of 8 octets.
	 * sizeof(struct ip6_frag) == 8
	 * sizeof(struct ip6_hdr) = 40
	 */
	if ((ip6f->ip6f_offlg & IP6F_MORE_FRAG) &&
	    (((ntohs(ip6->ip6_plen) - offset) & 0x7) != 0)) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offsetof(struct ip6_hdr, ip6_plen));
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	IP6_STATINC(IP6_STAT_FRAGMENTS);
	in6_ifstat_inc(dstifp, ifs6_reass_reqd);

	/* offset now points to data portion */
	offset += sizeof(struct ip6_frag);

	/*
	 * RFC6946:  A host that receives an IPv6 packet which includes 
	 * a Fragment Header with the "Fragmen Offset" equal to 0 and 
	 * the "M" bit equal to 0 MUST process such packet in isolation
	 *  from any other packets/fragments.
	 */
	fragoff = ntohs(ip6f->ip6f_offlg & IP6F_OFF_MASK);
	if (fragoff == 0 && !(ip6f->ip6f_offlg & IP6F_MORE_FRAG)) {
		IP6_STATINC(IP6_STAT_REASSEMBLED);
		in6_ifstat_inc(dstifp, ifs6_reass_ok);
		*offp = offset;
		return ip6f->ip6f_nxt;		
	}

	mutex_enter(&frag6_lock);

	/*
	 * Enforce upper bound on number of fragments.
	 * If maxfrag is 0, never accept fragments.
	 * If maxfrag is -1, accept all fragments without limitation.
	 */
	if (ip6_maxfrags < 0)
		;
	else if (frag6_nfrags >= (u_int)ip6_maxfrags)
		goto dropfrag;

	for (q6 = ip6q.ip6q_next; q6 != &ip6q; q6 = q6->ip6q_next)
		if (ip6f->ip6f_ident == q6->ip6q_ident &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &q6->ip6q_src) &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &q6->ip6q_dst))
			break;

	if (q6 == &ip6q) {
		/*
		 * the first fragment to arrive, create a reassembly queue.
		 */
		first_frag = 1;

		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly;
		 * If maxfragpackets is 0, never accept fragments.
		 * If maxfragpackets is -1, accept all fragments without
		 * limitation.
		 */
		if (ip6_maxfragpackets < 0)
			;
		else if (frag6_nfragpackets >= (u_int)ip6_maxfragpackets)
			goto dropfrag;
		frag6_nfragpackets++;

		q6 = kmem_intr_zalloc(sizeof(struct ip6q), KM_NOSLEEP);
		if (q6 == NULL) {
			goto dropfrag;
		}
		frag6_insque(q6, &ip6q);

		/* ip6q_nxt will be filled afterwards, from 1st fragment */
		q6->ip6q_down	= q6->ip6q_up = (struct ip6asfrag *)q6;
#ifdef notyet
		q6->ip6q_nxtp	= (u_char *)nxtp;
#endif
		q6->ip6q_ident	= ip6f->ip6f_ident;
		q6->ip6q_arrive = 0; /* Is it used anywhere? */
		q6->ip6q_ttl 	= IPV6_FRAGTTL;
		q6->ip6q_src	= ip6->ip6_src;
		q6->ip6q_dst	= ip6->ip6_dst;
		q6->ip6q_unfrglen = -1;	/* The 1st fragment has not arrived. */

		q6->ip6q_nfrag = 0;
	}

	/*
	 * If it's the 1st fragment, record the length of the
	 * unfragmentable part and the next header of the fragment header.
	 */

	if (fragoff == 0) {
		q6->ip6q_unfrglen = offset - sizeof(struct ip6_hdr) -
		    sizeof(struct ip6_frag);
		q6->ip6q_nxt = ip6f->ip6f_nxt;
	}

	/*
	 * Check that the reassembled packet would not exceed 65535 bytes
	 * in size.
	 * If it would exceed, discard the fragment and return an ICMP error.
	 */
	frgpartlen = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) - offset;
	if (q6->ip6q_unfrglen >= 0) {
		/* The 1st fragment has already arrived. */
		if (q6->ip6q_unfrglen + fragoff + frgpartlen > IPV6_MAXPACKET) {
			mutex_exit(&frag6_lock);
			icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) +
			    offsetof(struct ip6_frag, ip6f_offlg));
			return IPPROTO_DONE;
		}
	} else if (fragoff + frgpartlen > IPV6_MAXPACKET) {
		mutex_exit(&frag6_lock);
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) +
				offsetof(struct ip6_frag, ip6f_offlg));
		return IPPROTO_DONE;
	}
	/*
	 * If it's the first fragment, do the above check for each
	 * fragment already stored in the reassembly queue.
	 */
	if (fragoff == 0) {
		for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
		     af6 = af6dwn) {
			af6dwn = af6->ip6af_down;

			if (q6->ip6q_unfrglen + af6->ip6af_off + af6->ip6af_frglen >
			    IPV6_MAXPACKET) {
				struct mbuf *merr = IP6_REASS_MBUF(af6);
				struct ip6_hdr *ip6err;
				int erroff = af6->ip6af_offset;

				/* dequeue the fragment. */
				frag6_deq(af6);
				kmem_intr_free(af6, sizeof(struct ip6asfrag));

				/* adjust pointer. */
				ip6err = mtod(merr, struct ip6_hdr *);

				/*
				 * Restore source and destination addresses
				 * in the erroneous IPv6 header.
				 */
				ip6err->ip6_src = q6->ip6q_src;
				ip6err->ip6_dst = q6->ip6q_dst;

				icmp6_error(merr, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff - sizeof(struct ip6_frag) +
				    offsetof(struct ip6_frag, ip6f_offlg));
			}
		}
	}

	ip6af = kmem_intr_zalloc(sizeof(struct ip6asfrag), KM_NOSLEEP);
	if (ip6af == NULL) {
		goto dropfrag;
	}
	ip6af->ip6af_head = ip6->ip6_flow;
	ip6af->ip6af_len = ip6->ip6_plen;
	ip6af->ip6af_nxt = ip6->ip6_nxt;
	ip6af->ip6af_hlim = ip6->ip6_hlim;
	ip6af->ip6af_mff = ip6f->ip6f_offlg & IP6F_MORE_FRAG;
	ip6af->ip6af_off = fragoff;
	ip6af->ip6af_frglen = frgpartlen;
	ip6af->ip6af_offset = offset;
	IP6_REASS_MBUF(ip6af) = m;

	if (first_frag) {
		af6 = (struct ip6asfrag *)q6;
		goto insert;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = af6->ip6af_down)
		if (af6->ip6af_off > ip6af->ip6af_off)
			break;

	/*
	 * If the incoming fragment overlaps some existing fragments in
	 * the reassembly queue - drop it as per RFC 5722.
	 */
	if (af6->ip6af_up != (struct ip6asfrag *)q6) {
		i = af6->ip6af_up->ip6af_off + af6->ip6af_up->ip6af_frglen
			- ip6af->ip6af_off;
		if (i > 0) {
			kmem_intr_free(ip6af, sizeof(struct ip6asfrag));
			goto dropfrag;
		}
	}
	if (af6 != (struct ip6asfrag *)q6) {
		i = (ip6af->ip6af_off + ip6af->ip6af_frglen) - af6->ip6af_off;
		if (i > 0) {
			kmem_intr_free(ip6af, sizeof(struct ip6asfrag));
			goto dropfrag;
		}
	}

insert:

	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 * Move to front of packet queue, as we are
	 * the most recently active fragmented packet.
	 */
	frag6_enq(ip6af, af6->ip6af_up);
	frag6_nfrags++;
	q6->ip6q_nfrag++;
#if 0 /* xxx */
	if (q6 != ip6q.ip6q_next) {
		frag6_remque(q6);
		frag6_insque(q6, &ip6q);
	}
#endif
	next = 0;
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = af6->ip6af_down) {
		if (af6->ip6af_off != next) {
			mutex_exit(&frag6_lock);
			return IPPROTO_DONE;
		}
		next += af6->ip6af_frglen;
	}
	if (af6->ip6af_up->ip6af_mff) {
		mutex_exit(&frag6_lock);
		return IPPROTO_DONE;
	}

	/*
	 * Reassembly is complete; concatenate fragments.
	 */
	ip6af = q6->ip6q_down;
	t = m = IP6_REASS_MBUF(ip6af);
	af6 = ip6af->ip6af_down;
	frag6_deq(ip6af);
	while (af6 != (struct ip6asfrag *)q6) {
		af6dwn = af6->ip6af_down;
		frag6_deq(af6);
		while (t->m_next)
			t = t->m_next;
		t->m_next = IP6_REASS_MBUF(af6);
		m_adj(t->m_next, af6->ip6af_offset);
		kmem_intr_free(af6, sizeof(struct ip6asfrag));
		af6 = af6dwn;
	}

	/* adjust offset to point where the original next header starts */
	offset = ip6af->ip6af_offset - sizeof(struct ip6_frag);
	kmem_intr_free(ip6af, sizeof(struct ip6asfrag));
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons(next + offset - sizeof(struct ip6_hdr));
	ip6->ip6_src = q6->ip6q_src;
	ip6->ip6_dst = q6->ip6q_dst;
	nxt = q6->ip6q_nxt;
#ifdef notyet
	*q6->ip6q_nxtp = (u_char)(nxt & 0xff);
#endif

	/*
	 * Delete frag6 header with as a few cost as possible.
	 */
	if (m->m_len >= offset + sizeof(struct ip6_frag)) {
		memmove((char *)ip6 + sizeof(struct ip6_frag), ip6, offset);
		m->m_data += sizeof(struct ip6_frag);
		m->m_len -= sizeof(struct ip6_frag);
	} else {
		/* this comes with no copy if the boundary is on cluster */
		if ((t = m_split(m, offset, M_DONTWAIT)) == NULL) {
			frag6_remque(q6);
			frag6_nfrags -= q6->ip6q_nfrag;
			kmem_intr_free(q6, sizeof(struct ip6q));
			frag6_nfragpackets--;
			goto dropfrag;
		}
		m_adj(t, sizeof(struct ip6_frag));
		m_cat(m, t);
	}

	/*
	 * Store NXT to the original.
	 */
	{
		u_int8_t *prvnxtp = ip6_get_prevhdr(m, offset); /* XXX */
		*prvnxtp = nxt;
	}

	frag6_remque(q6);
	frag6_nfrags -= q6->ip6q_nfrag;
	kmem_intr_free(q6, sizeof(struct ip6q));
	frag6_nfragpackets--;

	if (m->m_flags & M_PKTHDR) { /* Isn't it always true? */
		int plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
	}

	IP6_STATINC(IP6_STAT_REASSEMBLED);
	in6_ifstat_inc(dstifp, ifs6_reass_ok);

	/*
	 * Tell launch routine the next header
	 */

	*mp = m;
	*offp = offset;

	mutex_exit(&frag6_lock);
	return nxt;

 dropfrag:
	mutex_exit(&frag6_lock);
	in6_ifstat_inc(dstifp, ifs6_reass_fail);
	IP6_STATINC(IP6_STAT_FRAGDROPPED);
	m_freem(m);
	return IPPROTO_DONE;
}

int
ip6_reass_packet(struct mbuf **mp, int offset)
{

	if (frag6_input(mp, &offset, IPPROTO_IPV6) == IPPROTO_DONE) {
		*mp = NULL;
		return EINVAL;
	}
	return 0;
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
void
frag6_freef(struct ip6q *q6)
{
	struct ip6asfrag *af6, *down6;

	KASSERT(mutex_owned(&frag6_lock));

	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = down6) {
		struct mbuf *m = IP6_REASS_MBUF(af6);

		down6 = af6->ip6af_down;
		frag6_deq(af6);

		/*
		 * Return ICMP time exceeded error for the 1st fragment.
		 * Just free other fragments.
		 */
		if (af6->ip6af_off == 0) {
			struct ip6_hdr *ip6;

			/* adjust pointer */
			ip6 = mtod(m, struct ip6_hdr *);

			/* restoure source and destination addresses */
			ip6->ip6_src = q6->ip6q_src;
			ip6->ip6_dst = q6->ip6q_dst;

			icmp6_error(m, ICMP6_TIME_EXCEEDED,
				    ICMP6_TIME_EXCEED_REASSEMBLY, 0);
		} else {
			m_freem(m);
		}
		kmem_intr_free(af6, sizeof(struct ip6asfrag));
	}
	frag6_remque(q6);
	frag6_nfrags -= q6->ip6q_nfrag;
	kmem_intr_free(q6, sizeof(struct ip6q));
	frag6_nfragpackets--;
}

/*
 * Put an ip fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
void
frag6_enq(struct ip6asfrag *af6, struct ip6asfrag *up6)
{

	KASSERT(mutex_owned(&frag6_lock));

	af6->ip6af_up = up6;
	af6->ip6af_down = up6->ip6af_down;
	up6->ip6af_down->ip6af_up = af6;
	up6->ip6af_down = af6;
}

/*
 * To frag6_enq as remque is to insque.
 */
void
frag6_deq(struct ip6asfrag *af6)
{

	KASSERT(mutex_owned(&frag6_lock));

	af6->ip6af_up->ip6af_down = af6->ip6af_down;
	af6->ip6af_down->ip6af_up = af6->ip6af_up;
}

void
frag6_insque(struct ip6q *newq, struct ip6q *oldq)
{

	KASSERT(mutex_owned(&frag6_lock));

	newq->ip6q_prev = oldq;
	newq->ip6q_next = oldq->ip6q_next;
	oldq->ip6q_next->ip6q_prev= newq;
	oldq->ip6q_next = newq;
}

void
frag6_remque(struct ip6q *p6)
{

	KASSERT(mutex_owned(&frag6_lock));

	p6->ip6q_prev->ip6q_next = p6->ip6q_next;
	p6->ip6q_next->ip6q_prev = p6->ip6q_prev;
}

void
frag6_fasttimo(void)
{
	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	if (frag6_drainwanted) {
		frag6_drain();
		frag6_drainwanted = 0;
	}

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * IPv6 reassembling timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
frag6_slowtimo(void)
{
	struct ip6q *q6;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	mutex_enter(&frag6_lock);
	q6 = ip6q.ip6q_next;
	if (q6)
		while (q6 != &ip6q) {
			--q6->ip6q_ttl;
			q6 = q6->ip6q_next;
			if (q6->ip6q_prev->ip6q_ttl == 0) {
				IP6_STATINC(IP6_STAT_FRAGTIMEOUT);
				/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
				frag6_freef(q6->ip6q_prev);
			}
		}
	/*
	 * If we are over the maximum number of fragments
	 * (due to the limit being lowered), drain off
	 * enough to get down to the new limit.
	 */
	while (frag6_nfragpackets > (u_int)ip6_maxfragpackets &&
	    ip6q.ip6q_prev) {
		IP6_STATINC(IP6_STAT_FRAGOVERFLOW);
		/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
		frag6_freef(ip6q.ip6q_prev);
	}
	mutex_exit(&frag6_lock);

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);

#if 0
	/*
	 * Routing changes might produce a better route than we last used;
	 * make sure we notice eventually, even if forwarding only for one
	 * destination and the cache is never replaced.
	 */
	rtcache_free(&ip6_forward_rt);
	rtcache_free(&ipsrcchk_rt);
#endif

}

void
frag6_drainstub(void)
{
	frag6_drainwanted = 1;
}

/*
 * Drain off all datagram fragments.
 */
void
frag6_drain(void)
{

	if (mutex_tryenter(&frag6_lock)) {
		while (ip6q.ip6q_next != &ip6q) {
			IP6_STATINC(IP6_STAT_FRAGDROPPED);
			/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
			frag6_freef(ip6q.ip6q_next);
		}
		mutex_exit(&frag6_lock);
	}
}
