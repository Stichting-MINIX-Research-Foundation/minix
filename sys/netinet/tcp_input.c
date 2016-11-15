/*	$NetBSD: tcp_input.c,v 1.344 2015/08/24 22:21:26 pooka Exp $	*/

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

/*
 *      @(#)COPYRIGHT   1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 *      This product includes software developed at the Information
 *      Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

/*-
 * Copyright (c) 1997, 1998, 1999, 2001, 2005, 2006,
 * 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Kevin M. Lahey of the Numerical Aerospace Simulation
 * Facility, NASA Ames Research Center.
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rui Paulo.
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
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 */

/*
 *	TODO list for SYN cache stuff:
 *
 *	Find room for a "state" field, which is needed to keep a
 *	compressed state for TIME_WAIT TCBs.  It's been noted already
 *	that this is fairly important for very high-volume web and
 *	mail servers, which use a large number of short-lived
 *	connections.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcp_input.c,v 1.344 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_ipsec.h"
#include "opt_inet_csum.h"
#include "opt_tcp_debug.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/pool.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#ifdef TCP_SIGNATURE
#include <sys/md5.h>
#endif
#include <sys/lwp.h> /* for lwp0 */
#include <sys/cprng.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/in_offload.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#ifdef TCP_SIGNATURE
#include <netinet6/scope6_var.h>
#endif
#endif

#ifndef INET6
/* always need ip6.h for IP6_EXTHDR_GET */
#include <netinet/ip6.h>
#endif

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_private.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_congctl.h>
#include <netinet/tcp_debug.h>

#ifdef INET6
#include "faith.h"
#if defined(NFAITH) && NFAITH > 0
#include <net/if_faith.h>
#endif
#endif	/* INET6 */

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec_var.h>
#include <netipsec/ipsec_private.h>
#include <netipsec/key.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#endif	/* IPSEC*/

#include <netinet/tcp_vtw.h>

int	tcprexmtthresh = 3;
int	tcp_log_refused;

int	tcp_do_autorcvbuf = 1;
int	tcp_autorcvbuf_inc = 16 * 1024;
int	tcp_autorcvbuf_max = 256 * 1024;
int	tcp_msl = (TCPTV_MSL / PR_SLOWHZ);

static int tcp_rst_ppslim_count = 0;
static struct timeval tcp_rst_ppslim_last;
static int tcp_ackdrop_ppslim_count = 0;
static struct timeval tcp_ackdrop_ppslim_last;

#define TCP_PAWS_IDLE	(24U * 24 * 60 * 60 * PR_SLOWHZ)

/* for modulo comparisons of timestamps */
#define TSTMP_LT(a,b)	((int)((a)-(b)) < 0)
#define TSTMP_GEQ(a,b)	((int)((a)-(b)) >= 0)

/*
 * Neighbor Discovery, Neighbor Unreachability Detection Upper layer hint.
 */
#ifdef INET6
static inline void
nd6_hint(struct tcpcb *tp)
{
	struct rtentry *rt;

	if (tp != NULL && tp->t_in6pcb != NULL && tp->t_family == AF_INET6 &&
	    (rt = rtcache_validate(&tp->t_in6pcb->in6p_route)) != NULL)
		nd6_nud_hint(rt);
}
#else
static inline void
nd6_hint(struct tcpcb *tp)
{
}
#endif

/*
 * Compute ACK transmission behavior.  Delay the ACK unless
 * we have already delayed an ACK (must send an ACK every two segments).
 * We also ACK immediately if we received a PUSH and the ACK-on-PUSH
 * option is enabled.
 */
static void
tcp_setup_ack(struct tcpcb *tp, const struct tcphdr *th)
{

	if (tp->t_flags & TF_DELACK ||
	    (tcp_ack_on_push && th->th_flags & TH_PUSH))
		tp->t_flags |= TF_ACKNOW;
	else
		TCP_SET_DELACK(tp);
}

static void
icmp_check(struct tcpcb *tp, const struct tcphdr *th, int acked)
{

	/*
	 * If we had a pending ICMP message that refers to data that have
	 * just been acknowledged, disregard the recorded ICMP message.
	 */
	if ((tp->t_flags & TF_PMTUD_PEND) &&
	    SEQ_GT(th->th_ack, tp->t_pmtud_th_seq))
		tp->t_flags &= ~TF_PMTUD_PEND;

	/*
	 * Keep track of the largest chunk of data
	 * acknowledged since last PMTU update
	 */
	if (tp->t_pmtud_mss_acked < acked)
		tp->t_pmtud_mss_acked = acked;
}

/*
 * Convert TCP protocol fields to host order for easier processing.
 */
static void
tcp_fields_to_host(struct tcphdr *th)
{

	NTOHL(th->th_seq);
	NTOHL(th->th_ack);
	NTOHS(th->th_win);
	NTOHS(th->th_urp);
}

/*
 * ... and reverse the above.
 */
static void
tcp_fields_to_net(struct tcphdr *th)
{

	HTONL(th->th_seq);
	HTONL(th->th_ack);
	HTONS(th->th_win);
	HTONS(th->th_urp);
}

#ifdef TCP_CSUM_COUNTERS
#include <sys/device.h>

#if defined(INET)
extern struct evcnt tcp_hwcsum_ok;
extern struct evcnt tcp_hwcsum_bad;
extern struct evcnt tcp_hwcsum_data;
extern struct evcnt tcp_swcsum;
#endif /* defined(INET) */
#if defined(INET6)
extern struct evcnt tcp6_hwcsum_ok;
extern struct evcnt tcp6_hwcsum_bad;
extern struct evcnt tcp6_hwcsum_data;
extern struct evcnt tcp6_swcsum;
#endif /* defined(INET6) */

#define	TCP_CSUM_COUNTER_INCR(ev)	(ev)->ev_count++

#else

#define	TCP_CSUM_COUNTER_INCR(ev)	/* nothing */

#endif /* TCP_CSUM_COUNTERS */

#ifdef TCP_REASS_COUNTERS
#include <sys/device.h>

extern struct evcnt tcp_reass_;
extern struct evcnt tcp_reass_empty;
extern struct evcnt tcp_reass_iteration[8];
extern struct evcnt tcp_reass_prependfirst;
extern struct evcnt tcp_reass_prepend;
extern struct evcnt tcp_reass_insert;
extern struct evcnt tcp_reass_inserttail;
extern struct evcnt tcp_reass_append;
extern struct evcnt tcp_reass_appendtail;
extern struct evcnt tcp_reass_overlaptail;
extern struct evcnt tcp_reass_overlapfront;
extern struct evcnt tcp_reass_segdup;
extern struct evcnt tcp_reass_fragdup;

#define	TCP_REASS_COUNTER_INCR(ev)	(ev)->ev_count++

#else

#define	TCP_REASS_COUNTER_INCR(ev)	/* nothing */

#endif /* TCP_REASS_COUNTERS */

static int tcp_reass(struct tcpcb *, const struct tcphdr *, struct mbuf *,
    int *);
static int tcp_dooptions(struct tcpcb *, const u_char *, int,
    struct tcphdr *, struct mbuf *, int, struct tcp_opt_info *);

#ifdef INET
static void tcp4_log_refused(const struct ip *, const struct tcphdr *);
#endif
#ifdef INET6
static void tcp6_log_refused(const struct ip6_hdr *, const struct tcphdr *);
#endif

#define	TRAVERSE(x) while ((x)->m_next) (x) = (x)->m_next

#if defined(MBUFTRACE)
struct mowner tcp_reass_mowner = MOWNER_INIT("tcp", "reass");
#endif /* defined(MBUFTRACE) */

static struct pool tcpipqent_pool;

void
tcpipqent_init(void)
{

	pool_init(&tcpipqent_pool, sizeof(struct ipqent), 0, 0, 0, "tcpipqepl",
	    NULL, IPL_VM);
}

struct ipqent *
tcpipqent_alloc(void)
{
	struct ipqent *ipqe;
	int s;

	s = splvm();
	ipqe = pool_get(&tcpipqent_pool, PR_NOWAIT);
	splx(s);

	return ipqe;
}

void
tcpipqent_free(struct ipqent *ipqe)
{
	int s;

	s = splvm();
	pool_put(&tcpipqent_pool, ipqe);
	splx(s);
}

static int
tcp_reass(struct tcpcb *tp, const struct tcphdr *th, struct mbuf *m, int *tlen)
{
	struct ipqent *p, *q, *nq, *tiqe = NULL;
	struct socket *so = NULL;
	int pkt_flags;
	tcp_seq pkt_seq;
	unsigned pkt_len;
	u_long rcvpartdupbyte = 0;
	u_long rcvoobyte;
#ifdef TCP_REASS_COUNTERS
	u_int count = 0;
#endif
	uint64_t *tcps;

	if (tp->t_inpcb)
		so = tp->t_inpcb->inp_socket;
#ifdef INET6
	else if (tp->t_in6pcb)
		so = tp->t_in6pcb->in6p_socket;
#endif

	TCP_REASS_LOCK_CHECK(tp);

	/*
	 * Call with th==0 after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (th == 0)
		goto present;

	m_claimm(m, &tcp_reass_mowner);

	rcvoobyte = *tlen;
	/*
	 * Copy these to local variables because the tcpiphdr
	 * gets munged while we are collapsing mbufs.
	 */
	pkt_seq = th->th_seq;
	pkt_len = *tlen;
	pkt_flags = th->th_flags;

	TCP_REASS_COUNTER_INCR(&tcp_reass_);

	if ((p = TAILQ_LAST(&tp->segq, ipqehead)) != NULL) {
		/*
		 * When we miss a packet, the vast majority of time we get
		 * packets that follow it in order.  So optimize for that.
		 */
		if (pkt_seq == p->ipqe_seq + p->ipqe_len) {
			p->ipqe_len += pkt_len;
			p->ipqe_flags |= pkt_flags;
			m_cat(p->ipre_mlast, m);
			TRAVERSE(p->ipre_mlast);
			m = NULL;
			tiqe = p;
			TAILQ_REMOVE(&tp->timeq, p, ipqe_timeq);
			TCP_REASS_COUNTER_INCR(&tcp_reass_appendtail);
			goto skip_replacement;
		}
		/*
		 * While we're here, if the pkt is completely beyond
		 * anything we have, just insert it at the tail.
		 */
		if (SEQ_GT(pkt_seq, p->ipqe_seq + p->ipqe_len)) {
			TCP_REASS_COUNTER_INCR(&tcp_reass_inserttail);
			goto insert_it;
		}
	}

	q = TAILQ_FIRST(&tp->segq);

	if (q != NULL) {
		/*
		 * If this segment immediately precedes the first out-of-order
		 * block, simply slap the segment in front of it and (mostly)
		 * skip the complicated logic.
		 */
		if (pkt_seq + pkt_len == q->ipqe_seq) {
			q->ipqe_seq = pkt_seq;
			q->ipqe_len += pkt_len;
			q->ipqe_flags |= pkt_flags;
			m_cat(m, q->ipqe_m);
			q->ipqe_m = m;
			q->ipre_mlast = m; /* last mbuf may have changed */
			TRAVERSE(q->ipre_mlast);
			tiqe = q;
			TAILQ_REMOVE(&tp->timeq, q, ipqe_timeq);
			TCP_REASS_COUNTER_INCR(&tcp_reass_prependfirst);
			goto skip_replacement;
		}
	} else {
		TCP_REASS_COUNTER_INCR(&tcp_reass_empty);
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL; q != NULL; q = nq) {
		nq = TAILQ_NEXT(q, ipqe_q);
#ifdef TCP_REASS_COUNTERS
		count++;
#endif
		/*
		 * If the received segment is just right after this
		 * fragment, merge the two together and then check
		 * for further overlaps.
		 */
		if (q->ipqe_seq + q->ipqe_len == pkt_seq) {
#ifdef TCPREASS_DEBUG
			printf("tcp_reass[%p]: concat %u:%u(%u) to %u:%u(%u)\n",
			       tp, pkt_seq, pkt_seq + pkt_len, pkt_len,
			       q->ipqe_seq, q->ipqe_seq + q->ipqe_len, q->ipqe_len);
#endif
			pkt_len += q->ipqe_len;
			pkt_flags |= q->ipqe_flags;
			pkt_seq = q->ipqe_seq;
			m_cat(q->ipre_mlast, m);
			TRAVERSE(q->ipre_mlast);
			m = q->ipqe_m;
			TCP_REASS_COUNTER_INCR(&tcp_reass_append);
			goto free_ipqe;
		}
		/*
		 * If the received segment is completely past this
		 * fragment, we need to go the next fragment.
		 */
		if (SEQ_LT(q->ipqe_seq + q->ipqe_len, pkt_seq)) {
			p = q;
			continue;
		}
		/*
		 * If the fragment is past the received segment,
		 * it (or any following) can't be concatenated.
		 */
		if (SEQ_GT(q->ipqe_seq, pkt_seq + pkt_len)) {
			TCP_REASS_COUNTER_INCR(&tcp_reass_insert);
			break;
		}

		/*
		 * We've received all the data in this segment before.
		 * mark it as a duplicate and return.
		 */
		if (SEQ_LEQ(q->ipqe_seq, pkt_seq) &&
		    SEQ_GEQ(q->ipqe_seq + q->ipqe_len, pkt_seq + pkt_len)) {
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVDUPPACK]++;
			tcps[TCP_STAT_RCVDUPBYTE] += pkt_len;
			TCP_STAT_PUTREF();
			tcp_new_dsack(tp, pkt_seq, pkt_len);
			m_freem(m);
			if (tiqe != NULL) {
				tcpipqent_free(tiqe);
			}
			TCP_REASS_COUNTER_INCR(&tcp_reass_segdup);
			goto out;
		}
		/*
		 * Received segment completely overlaps this fragment
		 * so we drop the fragment (this keeps the temporal
		 * ordering of segments correct).
		 */
		if (SEQ_GEQ(q->ipqe_seq, pkt_seq) &&
		    SEQ_LEQ(q->ipqe_seq + q->ipqe_len, pkt_seq + pkt_len)) {
			rcvpartdupbyte += q->ipqe_len;
			m_freem(q->ipqe_m);
			TCP_REASS_COUNTER_INCR(&tcp_reass_fragdup);
			goto free_ipqe;
		}
		/*
		 * RX'ed segment extends past the end of the
		 * fragment.  Drop the overlapping bytes.  Then
		 * merge the fragment and segment then treat as
		 * a longer received packet.
		 */
		if (SEQ_LT(q->ipqe_seq, pkt_seq) &&
		    SEQ_GT(q->ipqe_seq + q->ipqe_len, pkt_seq))  {
			int overlap = q->ipqe_seq + q->ipqe_len - pkt_seq;
#ifdef TCPREASS_DEBUG
			printf("tcp_reass[%p]: trim starting %d bytes of %u:%u(%u)\n",
			       tp, overlap,
			       pkt_seq, pkt_seq + pkt_len, pkt_len);
#endif
			m_adj(m, overlap);
			rcvpartdupbyte += overlap;
			m_cat(q->ipre_mlast, m);
			TRAVERSE(q->ipre_mlast);
			m = q->ipqe_m;
			pkt_seq = q->ipqe_seq;
			pkt_len += q->ipqe_len - overlap;
			rcvoobyte -= overlap;
			TCP_REASS_COUNTER_INCR(&tcp_reass_overlaptail);
			goto free_ipqe;
		}
		/*
		 * RX'ed segment extends past the front of the
		 * fragment.  Drop the overlapping bytes on the
		 * received packet.  The packet will then be
		 * contatentated with this fragment a bit later.
		 */
		if (SEQ_GT(q->ipqe_seq, pkt_seq) &&
		    SEQ_LT(q->ipqe_seq, pkt_seq + pkt_len))  {
			int overlap = pkt_seq + pkt_len - q->ipqe_seq;
#ifdef TCPREASS_DEBUG
			printf("tcp_reass[%p]: trim trailing %d bytes of %u:%u(%u)\n",
			       tp, overlap,
			       pkt_seq, pkt_seq + pkt_len, pkt_len);
#endif
			m_adj(m, -overlap);
			pkt_len -= overlap;
			rcvpartdupbyte += overlap;
			TCP_REASS_COUNTER_INCR(&tcp_reass_overlapfront);
			rcvoobyte -= overlap;
		}
		/*
		 * If the received segment immediates precedes this
		 * fragment then tack the fragment onto this segment
		 * and reinsert the data.
		 */
		if (q->ipqe_seq == pkt_seq + pkt_len) {
#ifdef TCPREASS_DEBUG
			printf("tcp_reass[%p]: append %u:%u(%u) to %u:%u(%u)\n",
			       tp, q->ipqe_seq, q->ipqe_seq + q->ipqe_len, q->ipqe_len,
			       pkt_seq, pkt_seq + pkt_len, pkt_len);
#endif
			pkt_len += q->ipqe_len;
			pkt_flags |= q->ipqe_flags;
			m_cat(m, q->ipqe_m);
			TAILQ_REMOVE(&tp->segq, q, ipqe_q);
			TAILQ_REMOVE(&tp->timeq, q, ipqe_timeq);
			tp->t_segqlen--;
			KASSERT(tp->t_segqlen >= 0);
			KASSERT(tp->t_segqlen != 0 ||
			    (TAILQ_EMPTY(&tp->segq) &&
			    TAILQ_EMPTY(&tp->timeq)));
			if (tiqe == NULL) {
				tiqe = q;
			} else {
				tcpipqent_free(q);
			}
			TCP_REASS_COUNTER_INCR(&tcp_reass_prepend);
			break;
		}
		/*
		 * If the fragment is before the segment, remember it.
		 * When this loop is terminated, p will contain the
		 * pointer to fragment that is right before the received
		 * segment.
		 */
		if (SEQ_LEQ(q->ipqe_seq, pkt_seq))
			p = q;

		continue;

		/*
		 * This is a common operation.  It also will allow
		 * to save doing a malloc/free in most instances.
		 */
	  free_ipqe:
		TAILQ_REMOVE(&tp->segq, q, ipqe_q);
		TAILQ_REMOVE(&tp->timeq, q, ipqe_timeq);
		tp->t_segqlen--;
		KASSERT(tp->t_segqlen >= 0);
		KASSERT(tp->t_segqlen != 0 ||
		    (TAILQ_EMPTY(&tp->segq) && TAILQ_EMPTY(&tp->timeq)));
		if (tiqe == NULL) {
			tiqe = q;
		} else {
			tcpipqent_free(q);
		}
	}

#ifdef TCP_REASS_COUNTERS
	if (count > 7)
		TCP_REASS_COUNTER_INCR(&tcp_reass_iteration[0]);
	else if (count > 0)
		TCP_REASS_COUNTER_INCR(&tcp_reass_iteration[count]);
#endif

    insert_it:

	/*
	 * Allocate a new queue entry since the received segment did not
	 * collapse onto any other out-of-order block; thus we are allocating
	 * a new block.  If it had collapsed, tiqe would not be NULL and
	 * we would be reusing it.
	 * XXX If we can't, just drop the packet.  XXX
	 */
	if (tiqe == NULL) {
		tiqe = tcpipqent_alloc();
		if (tiqe == NULL) {
			TCP_STATINC(TCP_STAT_RCVMEMDROP);
			m_freem(m);
			goto out;
		}
	}

	/*
	 * Update the counters.
	 */
	tp->t_rcvoopack++;
	tcps = TCP_STAT_GETREF();
	tcps[TCP_STAT_RCVOOPACK]++;
	tcps[TCP_STAT_RCVOOBYTE] += rcvoobyte;
	if (rcvpartdupbyte) {
	    tcps[TCP_STAT_RCVPARTDUPPACK]++;
	    tcps[TCP_STAT_RCVPARTDUPBYTE] += rcvpartdupbyte;
	}
	TCP_STAT_PUTREF();

	/*
	 * Insert the new fragment queue entry into both queues.
	 */
	tiqe->ipqe_m = m;
	tiqe->ipre_mlast = m;
	tiqe->ipqe_seq = pkt_seq;
	tiqe->ipqe_len = pkt_len;
	tiqe->ipqe_flags = pkt_flags;
	if (p == NULL) {
		TAILQ_INSERT_HEAD(&tp->segq, tiqe, ipqe_q);
#ifdef TCPREASS_DEBUG
		if (tiqe->ipqe_seq != tp->rcv_nxt)
			printf("tcp_reass[%p]: insert %u:%u(%u) at front\n",
			       tp, pkt_seq, pkt_seq + pkt_len, pkt_len);
#endif
	} else {
		TAILQ_INSERT_AFTER(&tp->segq, p, tiqe, ipqe_q);
#ifdef TCPREASS_DEBUG
		printf("tcp_reass[%p]: insert %u:%u(%u) after %u:%u(%u)\n",
		       tp, pkt_seq, pkt_seq + pkt_len, pkt_len,
		       p->ipqe_seq, p->ipqe_seq + p->ipqe_len, p->ipqe_len);
#endif
	}
	tp->t_segqlen++;

skip_replacement:

	TAILQ_INSERT_HEAD(&tp->timeq, tiqe, ipqe_timeq);

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		goto out;
	q = TAILQ_FIRST(&tp->segq);
	if (q == NULL || q->ipqe_seq != tp->rcv_nxt)
		goto out;
	if (tp->t_state == TCPS_SYN_RECEIVED && q->ipqe_len)
		goto out;

	tp->rcv_nxt += q->ipqe_len;
	pkt_flags = q->ipqe_flags & TH_FIN;
	nd6_hint(tp);

	TAILQ_REMOVE(&tp->segq, q, ipqe_q);
	TAILQ_REMOVE(&tp->timeq, q, ipqe_timeq);
	tp->t_segqlen--;
	KASSERT(tp->t_segqlen >= 0);
	KASSERT(tp->t_segqlen != 0 ||
	    (TAILQ_EMPTY(&tp->segq) && TAILQ_EMPTY(&tp->timeq)));
	if (so->so_state & SS_CANTRCVMORE)
		m_freem(q->ipqe_m);
	else
		sbappendstream(&so->so_rcv, q->ipqe_m);
	tcpipqent_free(q);
	TCP_REASS_UNLOCK(tp);
	sorwakeup(so);
	return (pkt_flags);
out:
	TCP_REASS_UNLOCK(tp);
	return (0);
}

#ifdef INET6
int
tcp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;

	/*
	 * draft-itojun-ipv6-tcp-to-anycast
	 * better place to put this in?
	 */
	if (m->m_flags & M_ANYCAST6) {
		struct ip6_hdr *ip6;
		if (m->m_len < sizeof(struct ip6_hdr)) {
			if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
				TCP_STATINC(TCP_STAT_RCVSHORT);
				return IPPROTO_DONE;
			}
		}
		ip6 = mtod(m, struct ip6_hdr *);
		icmp6_error(m, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADDR,
		    (char *)&ip6->ip6_dst - (char *)ip6);
		return IPPROTO_DONE;
	}

	tcp_input(m, *offp, proto);
	return IPPROTO_DONE;
}
#endif

#ifdef INET
static void
tcp4_log_refused(const struct ip *ip, const struct tcphdr *th)
{
	char src[INET_ADDRSTRLEN];
	char dst[INET_ADDRSTRLEN];

	if (ip) {
		in_print(src, sizeof(src), &ip->ip_src);
		in_print(dst, sizeof(dst), &ip->ip_dst);
	}
	else {
		strlcpy(src, "(unknown)", sizeof(src));
		strlcpy(dst, "(unknown)", sizeof(dst));
	}
	log(LOG_INFO,
	    "Connection attempt to TCP %s:%d from %s:%d\n",
	    dst, ntohs(th->th_dport),
	    src, ntohs(th->th_sport));
}
#endif

#ifdef INET6
static void
tcp6_log_refused(const struct ip6_hdr *ip6, const struct tcphdr *th)
{
	char src[INET6_ADDRSTRLEN];
	char dst[INET6_ADDRSTRLEN];

	if (ip6) {
		in6_print(src, sizeof(src), &ip6->ip6_src);
		in6_print(dst, sizeof(dst), &ip6->ip6_dst);
	}
	else {
		strlcpy(src, "(unknown v6)", sizeof(src));
		strlcpy(dst, "(unknown v6)", sizeof(dst));
	}
	log(LOG_INFO,
	    "Connection attempt to TCP [%s]:%d from [%s]:%d\n",
	    dst, ntohs(th->th_dport),
	    src, ntohs(th->th_sport));
}
#endif

/*
 * Checksum extended TCP header and data.
 */
int
tcp_input_checksum(int af, struct mbuf *m, const struct tcphdr *th,
    int toff, int off, int tlen)
{

	/*
	 * XXX it's better to record and check if this mbuf is
	 * already checked.
	 */

	switch (af) {
#ifdef INET
	case AF_INET:
		switch (m->m_pkthdr.csum_flags &
			((m->m_pkthdr.rcvif->if_csum_flags_rx & M_CSUM_TCPv4) |
			 M_CSUM_TCP_UDP_BAD | M_CSUM_DATA)) {
		case M_CSUM_TCPv4|M_CSUM_TCP_UDP_BAD:
			TCP_CSUM_COUNTER_INCR(&tcp_hwcsum_bad);
			goto badcsum;

		case M_CSUM_TCPv4|M_CSUM_DATA: {
			u_int32_t hw_csum = m->m_pkthdr.csum_data;

			TCP_CSUM_COUNTER_INCR(&tcp_hwcsum_data);
			if (m->m_pkthdr.csum_flags & M_CSUM_NO_PSEUDOHDR) {
				const struct ip *ip =
				    mtod(m, const struct ip *);

				hw_csum = in_cksum_phdr(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr,
				    htons(hw_csum + tlen + off + IPPROTO_TCP));
			}
			if ((hw_csum ^ 0xffff) != 0)
				goto badcsum;
			break;
		}

		case M_CSUM_TCPv4:
			/* Checksum was okay. */
			TCP_CSUM_COUNTER_INCR(&tcp_hwcsum_ok);
			break;

		default:
			/*
			 * Must compute it ourselves.  Maybe skip checksum
			 * on loopback interfaces.
			 */
			if (__predict_true(!(m->m_pkthdr.rcvif->if_flags &
					     IFF_LOOPBACK) ||
					   tcp_do_loopback_cksum)) {
				TCP_CSUM_COUNTER_INCR(&tcp_swcsum);
				if (in4_cksum(m, IPPROTO_TCP, toff,
					      tlen + off) != 0)
					goto badcsum;
			}
			break;
		}
		break;
#endif /* INET4 */

#ifdef INET6
	case AF_INET6:
		switch (m->m_pkthdr.csum_flags &
			((m->m_pkthdr.rcvif->if_csum_flags_rx & M_CSUM_TCPv6) |
			 M_CSUM_TCP_UDP_BAD | M_CSUM_DATA)) {
		case M_CSUM_TCPv6|M_CSUM_TCP_UDP_BAD:
			TCP_CSUM_COUNTER_INCR(&tcp6_hwcsum_bad);
			goto badcsum;

#if 0 /* notyet */
		case M_CSUM_TCPv6|M_CSUM_DATA:
#endif

		case M_CSUM_TCPv6:
			/* Checksum was okay. */
			TCP_CSUM_COUNTER_INCR(&tcp6_hwcsum_ok);
			break;

		default:
			/*
			 * Must compute it ourselves.  Maybe skip checksum
			 * on loopback interfaces.
			 */
			if (__predict_true((m->m_flags & M_LOOP) == 0 ||
			    tcp_do_loopback_cksum)) {
				TCP_CSUM_COUNTER_INCR(&tcp6_swcsum);
				if (in6_cksum(m, IPPROTO_TCP, toff,
				    tlen + off) != 0)
					goto badcsum;
			}
		}
		break;
#endif /* INET6 */
	}

	return 0;

badcsum:
	TCP_STATINC(TCP_STAT_RCVBADSUM);
	return -1;
}

/* When a packet arrives addressed to a vestigial tcpbp, we
 * nevertheless have to respond to it per the spec.
 */
static void tcp_vtw_input(struct tcphdr *th, vestigial_inpcb_t *vp,
			  struct mbuf *m, int tlen, int multicast)
{
	int		tiflags;
	int		todrop;
	uint32_t	t_flags = 0;
	uint64_t	*tcps;

	tiflags = th->th_flags;
	todrop  = vp->rcv_nxt - th->th_seq;

	if (todrop > 0) {
		if (tiflags & TH_SYN) {
			tiflags &= ~TH_SYN;
			++th->th_seq;
			if (th->th_urp > 1)
				--th->th_urp;
			else {
				tiflags &= ~TH_URG;
				th->th_urp = 0;
			}
			--todrop;
		}
		if (todrop > tlen ||
		    (todrop == tlen && (tiflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN or RST must be to the left of the
			 * window.  At this point the FIN or RST must be a
			 * duplicate or out of sequence; drop it.
			 */
			if (tiflags & TH_RST)
				goto drop;
			tiflags &= ~(TH_FIN|TH_RST);
			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 */
			t_flags |= TF_ACKNOW;
			todrop = tlen;
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVDUPPACK] += 1;
			tcps[TCP_STAT_RCVDUPBYTE] += todrop;
			TCP_STAT_PUTREF();
		} else if ((tiflags & TH_RST)
			   && th->th_seq != vp->rcv_nxt) {
			/*
			 * Test for reset before adjusting the sequence
			 * number for overlapping data.
			 */
			goto dropafterack_ratelim;
		} else {
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVPARTDUPPACK] += 1;
			tcps[TCP_STAT_RCVPARTDUPBYTE] += todrop;
			TCP_STAT_PUTREF();
		}

//		tcp_new_dsack(tp, th->th_seq, todrop);
//		hdroptlen += todrop;	/*drop from head afterwards*/

		th->th_seq += todrop;
		tlen -= todrop;

		if (th->th_urp > todrop)
			th->th_urp -= todrop;
		else {
			tiflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if (tlen) {
		TCP_STATINC(TCP_STAT_RCVAFTERCLOSE);
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (vp->rcv_nxt+vp->rcv_wnd);

	if (todrop > 0) {
		TCP_STATINC(TCP_STAT_RCVPACKAFTERWIN);
		if (todrop >= tlen) {
			/*
			 * The segment actually starts after the window.
			 * th->th_seq + tlen - vp->rcv_nxt - vp->rcv_wnd >= tlen
			 * th->th_seq - vp->rcv_nxt - vp->rcv_wnd >= 0
			 * th->th_seq >= vp->rcv_nxt + vp->rcv_wnd
			 */
			TCP_STATADD(TCP_STAT_RCVBYTEAFTERWIN, tlen);
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 */
			if ((tiflags & TH_SYN)
			    && SEQ_GT(th->th_seq, vp->rcv_nxt)) {
				/* We only support this in the !NOFDREF case, which
				 * is to say: not here.
				 */
				goto dropwithreset;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and (if not RST) ack.
			 */
			if (vp->rcv_wnd == 0 && th->th_seq == vp->rcv_nxt) {
				t_flags |= TF_ACKNOW;
				TCP_STATINC(TCP_STAT_RCVWINPROBE);
			} else
				goto dropafterack;
		} else
			TCP_STATADD(TCP_STAT_RCVBYTEAFTERWIN, todrop);
		m_adj(m, -todrop);
		tlen -= todrop;
		tiflags &= ~(TH_PUSH|TH_FIN);
	}

	if (tiflags & TH_RST) {
		if (th->th_seq != vp->rcv_nxt)
			goto dropafterack_ratelim;

		vtw_del(vp->ctl, vp->vtw);
		goto drop;
	}

	/*
	 * If the ACK bit is off we drop the segment and return.
	 */
	if ((tiflags & TH_ACK) == 0) {
		if (t_flags & TF_ACKNOW)
			goto dropafterack;
		else
			goto drop;
	}

	/*
	 * In TIME_WAIT state the only thing that should arrive
	 * is a retransmission of the remote FIN.  Acknowledge
	 * it and restart the finack timer.
	 */
	vtw_restart(vp);
	goto dropafterack;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 */
	if (tiflags & TH_RST)
		goto drop;
	goto dropafterack2;

dropafterack_ratelim:
	/*
	 * We may want to rate-limit ACKs against SYN/RST attack.
	 */
	if (ppsratecheck(&tcp_ackdrop_ppslim_last, &tcp_ackdrop_ppslim_count,
			 tcp_ackdrop_ppslim) == 0) {
		/* XXX stat */
		goto drop;
	}
	/* ...fall into dropafterack2... */

dropafterack2:
	(void)tcp_respond(0, m, m, th, th->th_seq + tlen, th->th_ack,
			  TH_ACK);
	return;

dropwithreset:
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 */
	if (tiflags & TH_RST)
		goto drop;

	if (tiflags & TH_ACK)
		tcp_respond(0, m, m, th, (tcp_seq)0, th->th_ack, TH_RST);
	else {
		if (tiflags & TH_SYN)
			++tlen;
		(void)tcp_respond(0, m, m, th, th->th_seq + tlen, (tcp_seq)0,
				  TH_RST|TH_ACK);
	}
	return;
drop:
	m_freem(m);
}

/*
 * TCP input routine, follows pages 65-76 of RFC 793 very closely.
 */
void
tcp_input(struct mbuf *m, ...)
{
	struct tcphdr *th;
	struct ip *ip;
	struct inpcb *inp;
#ifdef INET6
	struct ip6_hdr *ip6;
	struct in6pcb *in6p;
#endif
	u_int8_t *optp = NULL;
	int optlen = 0;
	int len, tlen, toff, hdroptlen = 0;
	struct tcpcb *tp = 0;
	int tiflags;
	struct socket *so = NULL;
	int todrop, acked, ourfinisacked, needoutput = 0;
	bool dupseg;
#ifdef TCP_DEBUG
	short ostate = 0;
#endif
	u_long tiwin;
	struct tcp_opt_info opti;
	int off, iphlen;
	va_list ap;
	int af;		/* af on the wire */
	struct mbuf *tcp_saveti = NULL;
	uint32_t ts_rtt;
	uint8_t iptos;
	uint64_t *tcps;
	vestigial_inpcb_t vestige;

	vestige.valid = 0;

	MCLAIM(m, &tcp_rx_mowner);
	va_start(ap, m);
	toff = va_arg(ap, int);
	(void)va_arg(ap, int);		/* ignore value, advance ap */
	va_end(ap);

	TCP_STATINC(TCP_STAT_RCVTOTAL);

	memset(&opti, 0, sizeof(opti));
	opti.ts_present = 0;
	opti.maxseg = 0;

	/*
	 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN.
	 *
	 * TCP is, by definition, unicast, so we reject all
	 * multicast outright.
	 *
	 * Note, there are additional src/dst address checks in
	 * the AF-specific code below.
	 */
	if (m->m_flags & (M_BCAST|M_MCAST)) {
		/* XXX stat */
		goto drop;
	}
#ifdef INET6
	if (m->m_flags & M_ANYCAST6) {
		/* XXX stat */
		goto drop;
	}
#endif

	/*
	 * Get IP and TCP header.
	 * Note: IP leaves IP header in first mbuf.
	 */
	ip = mtod(m, struct ip *);
	switch (ip->ip_v) {
#ifdef INET
	case 4:
#ifdef INET6
		ip6 = NULL;
#endif
		af = AF_INET;
		iphlen = sizeof(struct ip);
		IP6_EXTHDR_GET(th, struct tcphdr *, m, toff,
			sizeof(struct tcphdr));
		if (th == NULL) {
			TCP_STATINC(TCP_STAT_RCVSHORT);
			return;
		}
		/* We do the checksum after PCB lookup... */
		len = ntohs(ip->ip_len);
		tlen = len - toff;
		iptos = ip->ip_tos;
		break;
#endif
#ifdef INET6
	case 6:
		ip = NULL;
		iphlen = sizeof(struct ip6_hdr);
		af = AF_INET6;
		ip6 = mtod(m, struct ip6_hdr *);
		IP6_EXTHDR_GET(th, struct tcphdr *, m, toff,
			sizeof(struct tcphdr));
		if (th == NULL) {
			TCP_STATINC(TCP_STAT_RCVSHORT);
			return;
		}

		/* Be proactive about malicious use of IPv4 mapped address */
		if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
		    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
			/* XXX stat */
			goto drop;
		}

		/*
		 * Be proactive about unspecified IPv6 address in source.
		 * As we use all-zero to indicate unbounded/unconnected pcb,
		 * unspecified IPv6 address can be used to confuse us.
		 *
		 * Note that packets with unspecified IPv6 destination is
		 * already dropped in ip6_input.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
			/* XXX stat */
			goto drop;
		}

		/*
		 * Make sure destination address is not multicast.
		 * Source address checked in ip6_input().
		 */
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
			/* XXX stat */
			goto drop;
		}

		/* We do the checksum after PCB lookup... */
		len = m->m_pkthdr.len;
		tlen = len - toff;
		iptos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		break;
#endif
	default:
		m_freem(m);
		return;
	}

	KASSERT(TCP_HDR_ALIGNED_P(th));

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = th->th_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
		TCP_STATINC(TCP_STAT_RCVBADOFF);
		goto drop;
	}
	tlen -= off;

	/*
	 * tcp_input() has been modified to use tlen to mean the TCP data
	 * length throughout the function.  Other functions can use
	 * m->m_pkthdr.len as the basis for calculating the TCP data length.
	 * rja
	 */

	if (off > sizeof (struct tcphdr)) {
		IP6_EXTHDR_GET(th, struct tcphdr *, m, toff, off);
		if (th == NULL) {
			TCP_STATINC(TCP_STAT_RCVSHORT);
			return;
		}
		/*
		 * NOTE: ip/ip6 will not be affected by m_pulldown()
		 * (as they're before toff) and we don't need to update those.
		 */
		KASSERT(TCP_HDR_ALIGNED_P(th));
		optlen = off - sizeof (struct tcphdr);
		optp = ((u_int8_t *)th) + sizeof(struct tcphdr);
		/*
		 * Do quick retrieval of timestamp options ("options
		 * prediction?").  If timestamp is the only option and it's
		 * formatted as recommended in RFC 1323 appendix A, we
		 * quickly get the values now and not bother calling
		 * tcp_dooptions(), etc.
		 */
		if ((optlen == TCPOLEN_TSTAMP_APPA ||
		     (optlen > TCPOLEN_TSTAMP_APPA &&
			optp[TCPOLEN_TSTAMP_APPA] == TCPOPT_EOL)) &&
		     *(u_int32_t *)optp == htonl(TCPOPT_TSTAMP_HDR) &&
		     (th->th_flags & TH_SYN) == 0) {
			opti.ts_present = 1;
			opti.ts_val = ntohl(*(u_int32_t *)(optp + 4));
			opti.ts_ecr = ntohl(*(u_int32_t *)(optp + 8));
			optp = NULL;	/* we've parsed the options */
		}
	}
	tiflags = th->th_flags;

	/*
	 * Checksum extended TCP header and data
	 */
	if (tcp_input_checksum(af, m, th, toff, off, tlen))
		goto badcsum;

	/*
	 * Locate pcb for segment.
	 */
findpcb:
	inp = NULL;
#ifdef INET6
	in6p = NULL;
#endif
	switch (af) {
#ifdef INET
	case AF_INET:
		inp = in_pcblookup_connect(&tcbtable, ip->ip_src, th->th_sport,
					   ip->ip_dst, th->th_dport,
					   &vestige);
		if (inp == 0 && !vestige.valid) {
			TCP_STATINC(TCP_STAT_PCBHASHMISS);
			inp = in_pcblookup_bind(&tcbtable, ip->ip_dst, th->th_dport);
		}
#ifdef INET6
		if (inp == 0 && !vestige.valid) {
			struct in6_addr s, d;

			/* mapped addr case */
			memset(&s, 0, sizeof(s));
			s.s6_addr16[5] = htons(0xffff);
			bcopy(&ip->ip_src, &s.s6_addr32[3], sizeof(ip->ip_src));
			memset(&d, 0, sizeof(d));
			d.s6_addr16[5] = htons(0xffff);
			bcopy(&ip->ip_dst, &d.s6_addr32[3], sizeof(ip->ip_dst));
			in6p = in6_pcblookup_connect(&tcbtable, &s,
						     th->th_sport, &d, th->th_dport,
						     0, &vestige);
			if (in6p == 0 && !vestige.valid) {
				TCP_STATINC(TCP_STAT_PCBHASHMISS);
				in6p = in6_pcblookup_bind(&tcbtable, &d,
				    th->th_dport, 0);
			}
		}
#endif
#ifndef INET6
		if (inp == 0 && !vestige.valid)
#else
		if (inp == 0 && in6p == 0 && !vestige.valid)
#endif
		{
			TCP_STATINC(TCP_STAT_NOPORT);
			if (tcp_log_refused &&
			    (tiflags & (TH_RST|TH_ACK|TH_SYN)) == TH_SYN) {
				tcp4_log_refused(ip, th);
			}
			tcp_fields_to_host(th);
			goto dropwithreset_ratelim;
		}
#if defined(IPSEC)
		if (ipsec_used) {
			if (inp &&
			    (inp->inp_socket->so_options & SO_ACCEPTCONN) == 0
			    && ipsec4_in_reject(m, inp)) {
				IPSEC_STATINC(IPSEC_STAT_IN_POLVIO);
				goto drop;
			}
#ifdef INET6
			else if (in6p &&
			    (in6p->in6p_socket->so_options & SO_ACCEPTCONN) == 0
			    && ipsec6_in_reject_so(m, in6p->in6p_socket)) {
				IPSEC_STATINC(IPSEC_STAT_IN_POLVIO);
				goto drop;
			}
#endif
		}
#endif /*IPSEC*/
		break;
#endif /*INET*/
#ifdef INET6
	case AF_INET6:
	    {
		int faith;

#if defined(NFAITH) && NFAITH > 0
		faith = faithprefix(&ip6->ip6_dst);
#else
		faith = 0;
#endif
		in6p = in6_pcblookup_connect(&tcbtable, &ip6->ip6_src,
					     th->th_sport, &ip6->ip6_dst, th->th_dport, faith, &vestige);
		if (!in6p && !vestige.valid) {
			TCP_STATINC(TCP_STAT_PCBHASHMISS);
			in6p = in6_pcblookup_bind(&tcbtable, &ip6->ip6_dst,
				th->th_dport, faith);
		}
		if (!in6p && !vestige.valid) {
			TCP_STATINC(TCP_STAT_NOPORT);
			if (tcp_log_refused &&
			    (tiflags & (TH_RST|TH_ACK|TH_SYN)) == TH_SYN) {
				tcp6_log_refused(ip6, th);
			}
			tcp_fields_to_host(th);
			goto dropwithreset_ratelim;
		}
#if defined(IPSEC)
		if (ipsec_used && in6p
		    && (in6p->in6p_socket->so_options & SO_ACCEPTCONN) == 0
		    && ipsec6_in_reject(m, in6p)) {
			IPSEC6_STATINC(IPSEC_STAT_IN_POLVIO);
			goto drop;
		}
#endif /*IPSEC*/
		break;
	    }
#endif
	}

	/*
	 * If the state is CLOSED (i.e., TCB does not exist) then
	 * all data in the incoming segment is discarded.
	 * If the TCB exists but is in CLOSED state, it is embryonic,
	 * but should either do a listen or a connect soon.
	 */
	tp = NULL;
	so = NULL;
	if (inp) {
		/* Check the minimum TTL for socket. */
		if (ip->ip_ttl < inp->inp_ip_minttl)
			goto drop;

		tp = intotcpcb(inp);
		so = inp->inp_socket;
	}
#ifdef INET6
	else if (in6p) {
		tp = in6totcpcb(in6p);
		so = in6p->in6p_socket;
	}
#endif
	else if (vestige.valid) {
		int mc = 0;

		/* We do not support the resurrection of vtw tcpcps.
		 */
		if (tcp_input_checksum(af, m, th, toff, off, tlen))
			goto badcsum;

		switch (af) {
#ifdef INET6
		case AF_INET6:
			mc = IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst);
			break;
#endif

		case AF_INET:
			mc = (IN_MULTICAST(ip->ip_dst.s_addr)
			      || in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif));
			break;
		}

		tcp_fields_to_host(th);
		tcp_vtw_input(th, &vestige, m, tlen, mc);
		m = 0;
		goto drop;
	}

	if (tp == 0) {
		tcp_fields_to_host(th);
		goto dropwithreset_ratelim;
	}
	if (tp->t_state == TCPS_CLOSED)
		goto drop;

	KASSERT(so->so_lock == softnet_lock);
	KASSERT(solocked(so));

	tcp_fields_to_host(th);

	/* Unscale the window into a 32-bit value. */
	if ((tiflags & TH_SYN) == 0)
		tiwin = th->th_win << tp->snd_scale;
	else
		tiwin = th->th_win;

#ifdef INET6
	/* save packet options if user wanted */
	if (in6p && (in6p->in6p_flags & IN6P_CONTROLOPTS)) {
		if (in6p->in6p_options) {
			m_freem(in6p->in6p_options);
			in6p->in6p_options = 0;
		}
		KASSERT(ip6 != NULL);
		ip6_savecontrol(in6p, &in6p->in6p_options, ip6, m);
	}
#endif

	if (so->so_options & (SO_DEBUG|SO_ACCEPTCONN)) {
		union syn_cache_sa src;
		union syn_cache_sa dst;

		memset(&src, 0, sizeof(src));
		memset(&dst, 0, sizeof(dst));
		switch (af) {
#ifdef INET
		case AF_INET:
			src.sin.sin_len = sizeof(struct sockaddr_in);
			src.sin.sin_family = AF_INET;
			src.sin.sin_addr = ip->ip_src;
			src.sin.sin_port = th->th_sport;

			dst.sin.sin_len = sizeof(struct sockaddr_in);
			dst.sin.sin_family = AF_INET;
			dst.sin.sin_addr = ip->ip_dst;
			dst.sin.sin_port = th->th_dport;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			src.sin6.sin6_len = sizeof(struct sockaddr_in6);
			src.sin6.sin6_family = AF_INET6;
			src.sin6.sin6_addr = ip6->ip6_src;
			src.sin6.sin6_port = th->th_sport;

			dst.sin6.sin6_len = sizeof(struct sockaddr_in6);
			dst.sin6.sin6_family = AF_INET6;
			dst.sin6.sin6_addr = ip6->ip6_dst;
			dst.sin6.sin6_port = th->th_dport;
			break;
#endif /* INET6 */
		default:
			goto badsyn;	/*sanity*/
		}

		if (so->so_options & SO_DEBUG) {
#ifdef TCP_DEBUG
			ostate = tp->t_state;
#endif

			tcp_saveti = NULL;
			if (iphlen + sizeof(struct tcphdr) > MHLEN)
				goto nosave;

			if (m->m_len > iphlen && (m->m_flags & M_EXT) == 0) {
				tcp_saveti = m_copym(m, 0, iphlen, M_DONTWAIT);
				if (!tcp_saveti)
					goto nosave;
			} else {
				MGETHDR(tcp_saveti, M_DONTWAIT, MT_HEADER);
				if (!tcp_saveti)
					goto nosave;
				MCLAIM(m, &tcp_mowner);
				tcp_saveti->m_len = iphlen;
				m_copydata(m, 0, iphlen,
				    mtod(tcp_saveti, void *));
			}

			if (M_TRAILINGSPACE(tcp_saveti) < sizeof(struct tcphdr)) {
				m_freem(tcp_saveti);
				tcp_saveti = NULL;
			} else {
				tcp_saveti->m_len += sizeof(struct tcphdr);
				memcpy(mtod(tcp_saveti, char *) + iphlen, th,
				    sizeof(struct tcphdr));
			}
	nosave:;
		}
		if (so->so_options & SO_ACCEPTCONN) {
			if ((tiflags & (TH_RST|TH_ACK|TH_SYN)) != TH_SYN) {
				if (tiflags & TH_RST) {
					syn_cache_reset(&src.sa, &dst.sa, th);
				} else if ((tiflags & (TH_ACK|TH_SYN)) ==
				    (TH_ACK|TH_SYN)) {
					/*
					 * Received a SYN,ACK.  This should
					 * never happen while we are in
					 * LISTEN.  Send an RST.
					 */
					goto badsyn;
				} else if (tiflags & TH_ACK) {
					so = syn_cache_get(&src.sa, &dst.sa,
						th, toff, tlen, so, m);
					if (so == NULL) {
						/*
						 * We don't have a SYN for
						 * this ACK; send an RST.
						 */
						goto badsyn;
					} else if (so ==
					    (struct socket *)(-1)) {
						/*
						 * We were unable to create
						 * the connection.  If the
						 * 3-way handshake was
						 * completed, and RST has
						 * been sent to the peer.
						 * Since the mbuf might be
						 * in use for the reply,
						 * do not free it.
						 */
						m = NULL;
					} else {
						/*
						 * We have created a
						 * full-blown connection.
						 */
						tp = NULL;
						inp = NULL;
#ifdef INET6
						in6p = NULL;
#endif
						switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET
						case AF_INET:
							inp = sotoinpcb(so);
							tp = intotcpcb(inp);
							break;
#endif
#ifdef INET6
						case AF_INET6:
							in6p = sotoin6pcb(so);
							tp = in6totcpcb(in6p);
							break;
#endif
						}
						if (tp == NULL)
							goto badsyn;	/*XXX*/
						tiwin <<= tp->snd_scale;
						goto after_listen;
					}
				} else {
					/*
					 * None of RST, SYN or ACK was set.
					 * This is an invalid packet for a
					 * TCB in LISTEN state.  Send a RST.
					 */
					goto badsyn;
				}
			} else {
				/*
				 * Received a SYN.
				 *
				 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
				 */
				if (m->m_flags & (M_BCAST|M_MCAST))
					goto drop;

				switch (af) {
#ifdef INET6
				case AF_INET6:
					if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst))
						goto drop;
					break;
#endif /* INET6 */
				case AF_INET:
					if (IN_MULTICAST(ip->ip_dst.s_addr) ||
					    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
						goto drop;
				break;
				}

#ifdef INET6
				/*
				 * If deprecated address is forbidden, we do
				 * not accept SYN to deprecated interface
				 * address to prevent any new inbound
				 * connection from getting established.
				 * When we do not accept SYN, we send a TCP
				 * RST, with deprecated source address (instead
				 * of dropping it).  We compromise it as it is
				 * much better for peer to send a RST, and
				 * RST will be the final packet for the
				 * exchange.
				 *
				 * If we do not forbid deprecated addresses, we
				 * accept the SYN packet.  RFC2462 does not
				 * suggest dropping SYN in this case.
				 * If we decipher RFC2462 5.5.4, it says like
				 * this:
				 * 1. use of deprecated addr with existing
				 *    communication is okay - "SHOULD continue
				 *    to be used"
				 * 2. use of it with new communication:
				 *   (2a) "SHOULD NOT be used if alternate
				 *        address with sufficient scope is
				 *        available"
				 *   (2b) nothing mentioned otherwise.
				 * Here we fall into (2b) case as we have no
				 * choice in our source address selection - we
				 * must obey the peer.
				 *
				 * The wording in RFC2462 is confusing, and
				 * there are multiple description text for
				 * deprecated address handling - worse, they
				 * are not exactly the same.  I believe 5.5.4
				 * is the best one, so we follow 5.5.4.
				 */
				if (af == AF_INET6 && !ip6_use_deprecated) {
					struct in6_ifaddr *ia6;
					if ((ia6 = in6ifa_ifpwithaddr(m->m_pkthdr.rcvif,
					    &ip6->ip6_dst)) &&
					    (ia6->ia6_flags & IN6_IFF_DEPRECATED)) {
						tp = NULL;
						goto dropwithreset;
					}
				}
#endif

#if defined(IPSEC)
				if (ipsec_used) {
					switch (af) {
#ifdef INET
					case AF_INET:
						if (!ipsec4_in_reject_so(m, so))
							break;
						IPSEC_STATINC(
						    IPSEC_STAT_IN_POLVIO);
						tp = NULL;
						goto dropwithreset;
#endif
#ifdef INET6
					case AF_INET6:
						if (!ipsec6_in_reject_so(m, so))
							break;
						IPSEC6_STATINC(
						    IPSEC_STAT_IN_POLVIO);
						tp = NULL;
						goto dropwithreset;
#endif /*INET6*/
					}
				}
#endif /*IPSEC*/

				/*
				 * LISTEN socket received a SYN
				 * from itself?  This can't possibly
				 * be valid; drop the packet.
				 */
				if (th->th_sport == th->th_dport) {
					int i;

					switch (af) {
#ifdef INET
					case AF_INET:
						i = in_hosteq(ip->ip_src, ip->ip_dst);
						break;
#endif
#ifdef INET6
					case AF_INET6:
						i = IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &ip6->ip6_dst);
						break;
#endif
					default:
						i = 1;
					}
					if (i) {
						TCP_STATINC(TCP_STAT_BADSYN);
						goto drop;
					}
				}

				/*
				 * SYN looks ok; create compressed TCP
				 * state for it.
				 */
				if (so->so_qlen <= so->so_qlimit &&
				    syn_cache_add(&src.sa, &dst.sa, th, tlen,
						so, m, optp, optlen, &opti))
					m = NULL;
			}
			goto drop;
		}
	}

after_listen:
#ifdef DIAGNOSTIC
	/*
	 * Should not happen now that all embryonic connections
	 * are handled with compressed state.
	 */
	if (tp->t_state == TCPS_LISTEN)
		panic("tcp_input: TCPS_LISTEN");
#endif

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	tp->t_rcvtime = tcp_now;
	if (TCPS_HAVEESTABLISHED(tp->t_state))
		TCP_TIMER_ARM(tp, TCPT_KEEP, tp->t_keepidle);

	/*
	 * Process options.
	 */
#ifdef TCP_SIGNATURE
	if (optp || (tp->t_flags & TF_SIGNATURE))
#else
	if (optp)
#endif
		if (tcp_dooptions(tp, optp, optlen, th, m, toff, &opti) < 0)
			goto drop;

	if (TCP_SACK_ENABLED(tp)) {
		tcp_del_sackholes(tp, th);
	}

	if (TCP_ECN_ALLOWED(tp)) {
		if (tiflags & TH_CWR) {
			tp->t_flags &= ~TF_ECN_SND_ECE;
		}
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			tp->t_flags |= TF_ECN_SND_ECE;
			TCP_STATINC(TCP_STAT_ECN_CE);
			break;
		case IPTOS_ECN_ECT0:
			TCP_STATINC(TCP_STAT_ECN_ECT);
			break;
		case IPTOS_ECN_ECT1:
			/* XXX: ignore for now -- rpaulo */
			break;
		}
		/*
		 * Congestion experienced.
		 * Ignore if we are already trying to recover.
		 */
		if ((tiflags & TH_ECE) && SEQ_GEQ(tp->snd_una, tp->snd_recover))
			tp->t_congctl->cong_exp(tp);
	}

	if (opti.ts_present && opti.ts_ecr) {
		/*
		 * Calculate the RTT from the returned time stamp and the
		 * connection's time base.  If the time stamp is later than
		 * the current time, or is extremely old, fall back to non-1323
		 * RTT calculation.  Since ts_rtt is unsigned, we can test both
		 * at the same time.
		 *
		 * Note that ts_rtt is in units of slow ticks (500
		 * ms).  Since most earthbound RTTs are < 500 ms,
		 * observed values will have large quantization noise.
		 * Our smoothed RTT is then the fraction of observed
		 * samples that are 1 tick instead of 0 (times 500
		 * ms).
		 *
		 * ts_rtt is increased by 1 to denote a valid sample,
		 * with 0 indicating an invalid measurement.  This
		 * extra 1 must be removed when ts_rtt is used, or
		 * else an an erroneous extra 500 ms will result.
		 */
		ts_rtt = TCP_TIMESTAMP(tp) - opti.ts_ecr + 1;
		if (ts_rtt > TCP_PAWS_IDLE)
			ts_rtt = 0;
	} else {
		ts_rtt = 0;
	}

	/*
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ECE|TH_CWR|TH_ACK))
	        == TH_ACK &&
	    (!opti.ts_present || TSTMP_GEQ(opti.ts_val, tp->ts_recent)) &&
	    th->th_seq == tp->rcv_nxt &&
	    tiwin && tiwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {

		/*
		 * If last ACK falls within this segment's sequence numbers,
		 * record the timestamp.
		 * NOTE that the test is modified according to the latest
		 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
		 *
		 * note that we already know
		 *	TSTMP_GEQ(opti.ts_val, tp->ts_recent)
		 */
		if (opti.ts_present &&
		    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = tcp_now;
			tp->ts_recent = opti.ts_val;
		}

		if (tlen == 0) {
			/* Ack prediction. */
			if (SEQ_GT(th->th_ack, tp->snd_una) &&
			    SEQ_LEQ(th->th_ack, tp->snd_max) &&
			    tp->snd_cwnd >= tp->snd_wnd &&
			    tp->t_partialacks < 0) {
				/*
				 * this is a pure ack for outstanding data.
				 */
				if (ts_rtt)
					tcp_xmit_timer(tp, ts_rtt - 1);
				else if (tp->t_rtttime &&
				    SEQ_GT(th->th_ack, tp->t_rtseq))
					tcp_xmit_timer(tp,
					  tcp_now - tp->t_rtttime);
				acked = th->th_ack - tp->snd_una;
				tcps = TCP_STAT_GETREF();
				tcps[TCP_STAT_PREDACK]++;
				tcps[TCP_STAT_RCVACKPACK]++;
				tcps[TCP_STAT_RCVACKBYTE] += acked;
				TCP_STAT_PUTREF();
				nd6_hint(tp);

				if (acked > (tp->t_lastoff - tp->t_inoff))
					tp->t_lastm = NULL;
				sbdrop(&so->so_snd, acked);
				tp->t_lastoff -= acked;

				icmp_check(tp, th, acked);

				tp->snd_una = th->th_ack;
				tp->snd_fack = tp->snd_una;
				if (SEQ_LT(tp->snd_high, tp->snd_una))
					tp->snd_high = tp->snd_una;
				m_freem(m);

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selnotify/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
				if (tp->snd_una == tp->snd_max)
					TCP_TIMER_DISARM(tp, TCPT_REXMT);
				else if (TCP_TIMER_ISARMED(tp,
				    TCPT_PERSIST) == 0)
					TCP_TIMER_ARM(tp, TCPT_REXMT,
					    tp->t_rxtcur);

				sowwakeup(so);
				if (so->so_snd.sb_cc) {
					KERNEL_LOCK(1, NULL);
					(void) tcp_output(tp);
					KERNEL_UNLOCK_ONE(NULL);
				}
				if (tcp_saveti)
					m_freem(tcp_saveti);
				return;
			}
		} else if (th->th_ack == tp->snd_una &&
		    TAILQ_FIRST(&tp->segq) == NULL &&
		    tlen <= sbspace(&so->so_rcv)) {
			int newsize = 0;	/* automatic sockbuf scaling */

			/*
			 * this is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */
			tp->rcv_nxt += tlen;
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_PREDDAT]++;
			tcps[TCP_STAT_RCVPACK]++;
			tcps[TCP_STAT_RCVBYTE] += tlen;
			TCP_STAT_PUTREF();
			nd6_hint(tp);

		/*
		 * Automatic sizing enables the performance of large buffers
		 * and most of the efficiency of small ones by only allocating
		 * space when it is needed.
		 *
		 * On the receive side the socket buffer memory is only rarely
		 * used to any significant extent.  This allows us to be much
		 * more aggressive in scaling the receive socket buffer.  For
		 * the case that the buffer space is actually used to a large
		 * extent and we run out of kernel memory we can simply drop
		 * the new segments; TCP on the sender will just retransmit it
		 * later.  Setting the buffer size too big may only consume too
		 * much kernel memory if the application doesn't read() from
		 * the socket or packet loss or reordering makes use of the
		 * reassembly queue.
		 *
		 * The criteria to step up the receive buffer one notch are:
		 *  1. the number of bytes received during the time it takes
		 *     one timestamp to be reflected back to us (the RTT);
		 *  2. received bytes per RTT is within seven eighth of the
		 *     current socket buffer size;
		 *  3. receive buffer size has not hit maximal automatic size;
		 *
		 * This algorithm does one step per RTT at most and only if
		 * we receive a bulk stream w/o packet losses or reorderings.
		 * Shrinking the buffer during idle times is not necessary as
		 * it doesn't consume any memory when idle.
		 *
		 * TODO: Only step up if the application is actually serving
		 * the buffer to better manage the socket buffer resources.
		 */
			if (tcp_do_autorcvbuf &&
			    opti.ts_ecr &&
			    (so->so_rcv.sb_flags & SB_AUTOSIZE)) {
				if (opti.ts_ecr > tp->rfbuf_ts &&
				    opti.ts_ecr - tp->rfbuf_ts < PR_SLOWHZ) {
					if (tp->rfbuf_cnt >
					    (so->so_rcv.sb_hiwat / 8 * 7) &&
					    so->so_rcv.sb_hiwat <
					    tcp_autorcvbuf_max) {
						newsize =
						    min(so->so_rcv.sb_hiwat +
						    tcp_autorcvbuf_inc,
						    tcp_autorcvbuf_max);
					}
					/* Start over with next RTT. */
					tp->rfbuf_ts = 0;
					tp->rfbuf_cnt = 0;
				} else
					tp->rfbuf_cnt += tlen;	/* add up */
			}

			/*
			 * Drop TCP, IP headers and TCP options then add data
			 * to socket buffer.
			 */
			if (so->so_state & SS_CANTRCVMORE)
				m_freem(m);
			else {
				/*
				 * Set new socket buffer size.
				 * Give up when limit is reached.
				 */
				if (newsize)
					if (!sbreserve(&so->so_rcv,
					    newsize, so))
						so->so_rcv.sb_flags &= ~SB_AUTOSIZE;
				m_adj(m, toff + off);
				sbappendstream(&so->so_rcv, m);
			}
			sorwakeup(so);
			tcp_setup_ack(tp, th);
			if (tp->t_flags & TF_ACKNOW) {
				KERNEL_LOCK(1, NULL);
				(void) tcp_output(tp);
				KERNEL_UNLOCK_ONE(NULL);
			}
			if (tcp_saveti)
				m_freem(tcp_saveti);
			return;
		}
	}

	/*
	 * Compute mbuf offset to TCP data segment.
	 */
	hdroptlen = toff + off;

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	{ int win;

	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));
	}

	/* Reset receive buffer auto scaling when not in bulk receive mode. */
	tp->rfbuf_ts = 0;
	tp->rfbuf_cnt = 0;

	switch (tp->t_state) {
	/*
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if seg contains a ECE and ECN support is enabled, the stream
	 *	    is ECN capable.
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if ((tiflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		     SEQ_GT(th->th_ack, tp->snd_max)))
			goto dropwithreset;
		if (tiflags & TH_RST) {
			if (tiflags & TH_ACK)
				tp = tcp_drop(tp, ECONNREFUSED);
			goto drop;
		}
		if ((tiflags & TH_SYN) == 0)
			goto drop;
		if (tiflags & TH_ACK) {
			tp->snd_una = th->th_ack;
			if (SEQ_LT(tp->snd_nxt, tp->snd_una))
				tp->snd_nxt = tp->snd_una;
			if (SEQ_LT(tp->snd_high, tp->snd_una))
				tp->snd_high = tp->snd_una;
			TCP_TIMER_DISARM(tp, TCPT_REXMT);

			if ((tiflags & TH_ECE) && tcp_do_ecn) {
				tp->t_flags |= TF_ECN_PERMIT;
				TCP_STATINC(TCP_STAT_ECN_SHS);
			}

		}
		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		tcp_mss_from_peer(tp, opti.maxseg);

		/*
		 * Initialize the initial congestion window.  If we
		 * had to retransmit the SYN, we must initialize cwnd
		 * to 1 segment (i.e. the Loss Window).
		 */
		if (tp->t_flags & TF_SYN_REXMT)
			tp->snd_cwnd = tp->t_peermss;
		else {
			int ss = tcp_init_win;
#ifdef INET
			if (inp != NULL && in_localaddr(inp->inp_faddr))
				ss = tcp_init_win_local;
#endif
#ifdef INET6
			if (in6p != NULL && in6_localaddr(&in6p->in6p_faddr))
				ss = tcp_init_win_local;
#endif
			tp->snd_cwnd = TCP_INITIAL_WINDOW(ss, tp->t_peermss);
		}

		tcp_rmx_rtt(tp);
		if (tiflags & TH_ACK) {
			TCP_STATINC(TCP_STAT_CONNECTS);
			/*
			 * move tcp_established before soisconnected
			 * because upcall handler can drive tcp_output
			 * functionality.
			 * XXX we might call soisconnected at the end of
			 * all processing
			 */
			tcp_established(tp);
			soisconnected(so);
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			    (TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
			TCP_REASS_LOCK(tp);
			(void) tcp_reass(tp, NULL, NULL, &tlen);
			/*
			 * if we didn't have to retransmit the SYN,
			 * use its rtt as our initial srtt & rtt var.
			 */
			if (tp->t_rtttime)
				tcp_xmit_timer(tp, tcp_now - tp->t_rtttime);
		} else
			tp->t_state = TCPS_SYN_RECEIVED;

		/*
		 * Advance th->th_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		th->th_seq++;
		if (tlen > tp->rcv_wnd) {
			todrop = tlen - tp->rcv_wnd;
			m_adj(m, -todrop);
			tlen = tp->rcv_wnd;
			tiflags &= ~TH_FIN;
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVPACKAFTERWIN]++;
			tcps[TCP_STAT_RCVBYTEAFTERWIN] += todrop;
			TCP_STAT_PUTREF();
		}
		tp->snd_wl1 = th->th_seq - 1;
		tp->rcv_up = th->th_seq;
		goto step6;

	/*
	 * If the state is SYN_RECEIVED:
	 *	If seg contains an ACK, but not for our SYN, drop the input
	 *	and generate an RST.  See page 36, rfc793
	 */
	case TCPS_SYN_RECEIVED:
		if ((tiflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		     SEQ_GT(th->th_ack, tp->snd_max)))
			goto dropwithreset;
		break;
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check timestamp, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 *
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if (opti.ts_present && (tiflags & TH_RST) == 0 && tp->ts_recent &&
	    TSTMP_LT(opti.ts_val, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if (tcp_now - tp->ts_recent_age > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVDUPPACK]++;
			tcps[TCP_STAT_RCVDUPBYTE] += tlen;
			tcps[TCP_STAT_PAWSDROP]++;
			TCP_STAT_PUTREF();
			tcp_new_dsack(tp, th->th_seq, tlen);
			goto dropafterack;
		}
	}

	todrop = tp->rcv_nxt - th->th_seq;
	dupseg = false;
	if (todrop > 0) {
		if (tiflags & TH_SYN) {
			tiflags &= ~TH_SYN;
			th->th_seq++;
			if (th->th_urp > 1)
				th->th_urp--;
			else {
				tiflags &= ~TH_URG;
				th->th_urp = 0;
			}
			todrop--;
		}
		if (todrop > tlen ||
		    (todrop == tlen && (tiflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN or RST must be to the left of the
			 * window.  At this point the FIN or RST must be a
			 * duplicate or out of sequence; drop it.
			 */
			if (tiflags & TH_RST)
				goto drop;
			tiflags &= ~(TH_FIN|TH_RST);
			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			todrop = tlen;
			dupseg = true;
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVDUPPACK]++;
			tcps[TCP_STAT_RCVDUPBYTE] += todrop;
			TCP_STAT_PUTREF();
		} else if ((tiflags & TH_RST) &&
			   th->th_seq != tp->rcv_nxt) {
			/*
			 * Test for reset before adjusting the sequence
			 * number for overlapping data.
			 */
			goto dropafterack_ratelim;
		} else {
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVPARTDUPPACK]++;
			tcps[TCP_STAT_RCVPARTDUPBYTE] += todrop;
			TCP_STAT_PUTREF();
		}
		tcp_new_dsack(tp, th->th_seq, todrop);
		hdroptlen += todrop;	/*drop from head afterwards*/
		th->th_seq += todrop;
		tlen -= todrop;
		if (th->th_urp > todrop)
			th->th_urp -= todrop;
		else {
			tiflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && tlen) {
		tp = tcp_close(tp);
		TCP_STATINC(TCP_STAT_RCVAFTERCLOSE);
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt+tp->rcv_wnd);
	if (todrop > 0) {
		TCP_STATINC(TCP_STAT_RCVPACKAFTERWIN);
		if (todrop >= tlen) {
			/*
			 * The segment actually starts after the window.
			 * th->th_seq + tlen - tp->rcv_nxt - tp->rcv_wnd >= tlen
			 * th->th_seq - tp->rcv_nxt - tp->rcv_wnd >= 0
			 * th->th_seq >= tp->rcv_nxt + tp->rcv_wnd
			 */
			TCP_STATADD(TCP_STAT_RCVBYTEAFTERWIN, tlen);
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 *
			 * NOTE: We will checksum the packet again, and
			 * so we need to put the header fields back into
			 * network order!
			 * XXX This kind of sucks, but we don't expect
			 * XXX this to happen very often, so maybe it
			 * XXX doesn't matter so much.
			 */
			if (tiflags & TH_SYN &&
			    tp->t_state == TCPS_TIME_WAIT &&
			    SEQ_GT(th->th_seq, tp->rcv_nxt)) {
				tp = tcp_close(tp);
				tcp_fields_to_net(th);
				goto findpcb;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and (if not RST) ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				TCP_STATINC(TCP_STAT_RCVWINPROBE);
			} else
				goto dropafterack;
		} else
			TCP_STATADD(TCP_STAT_RCVBYTEAFTERWIN, todrop);
		m_adj(m, -todrop);
		tlen -= todrop;
		tiflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 *  record the timestamp.
	 * NOTE: 
	 * 1) That the test incorporates suggestions from the latest
	 *    proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 * 2) That updating only on newer timestamps interferes with
	 *    our earlier PAWS tests, so this check should be solely
	 *    predicated on the sequence space of this segment.
	 * 3) That we modify the segment boundary check to be 
	 *        Last.ACK.Sent <= SEG.SEQ + SEG.Len  
	 *    instead of RFC1323's
	 *        Last.ACK.Sent < SEG.SEQ + SEG.Len,
	 *    This modified check allows us to overcome RFC1323's
	 *    limitations as described in Stevens TCP/IP Illustrated
	 *    Vol. 2 p.869. In such cases, we can still calculate the
	 *    RTT correctly when RCV.NXT == Last.ACK.Sent.
	 */
	if (opti.ts_present &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
		    ((tiflags & (TH_SYN|TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_now;
		tp->ts_recent = opti.ts_val;
	}

	/*
	 * If the RST bit is set examine the state:
	 *    SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *    CLOSING, LAST_ACK, TIME_WAIT STATES
	 *	Close the tcb.
	 */
	if (tiflags & TH_RST) {
		if (th->th_seq != tp->rcv_nxt)
			goto dropafterack_ratelim;

		switch (tp->t_state) {
		case TCPS_SYN_RECEIVED:
			so->so_error = ECONNREFUSED;
			goto close;

		case TCPS_ESTABLISHED:
		case TCPS_FIN_WAIT_1:
		case TCPS_FIN_WAIT_2:
		case TCPS_CLOSE_WAIT:
			so->so_error = ECONNRESET;
		close:
			tp->t_state = TCPS_CLOSED;
			TCP_STATINC(TCP_STAT_DROPS);
			tp = tcp_close(tp);
			goto drop;

		case TCPS_CLOSING:
		case TCPS_LAST_ACK:
		case TCPS_TIME_WAIT:
			tp = tcp_close(tp);
			goto drop;
		}
	}

	/*
	 * Since we've covered the SYN-SENT and SYN-RECEIVED states above
	 * we must be in a synchronized state.  RFC791 states (under RST
	 * generation) that any unacceptable segment (an out-of-order SYN
	 * qualifies) received in a synchronized state must elicit only an
	 * empty acknowledgment segment ... and the connection remains in
	 * the same state.
	 */
	if (tiflags & TH_SYN) {
		if (tp->rcv_nxt == th->th_seq) {
			tcp_respond(tp, m, m, th, (tcp_seq)0, th->th_ack - 1,
			    TH_ACK);
			if (tcp_saveti)
				m_freem(tcp_saveti);
			return;
		}

		goto dropafterack_ratelim;
	}

	/*
	 * If the ACK bit is off we drop the segment and return.
	 */
	if ((tiflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_ACKNOW)
			goto dropafterack;
		else
			goto drop;
	}

	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {

	/*
	 * In SYN_RECEIVED state if the ack ACKs our SYN then enter
	 * ESTABLISHED state and continue processing, otherwise
	 * send an RST.
	 */
	case TCPS_SYN_RECEIVED:
		if (SEQ_GT(tp->snd_una, th->th_ack) ||
		    SEQ_GT(th->th_ack, tp->snd_max))
			goto dropwithreset;
		TCP_STATINC(TCP_STAT_CONNECTS);
		soisconnected(so);
		tcp_established(tp);
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
		    (TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		TCP_REASS_LOCK(tp);
		(void) tcp_reass(tp, NULL, NULL, &tlen);
		tp->snd_wl1 = th->th_seq - 1;
		/* fall into ... */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < th->th_ack <= tp->snd_max
	 * then advance tp->snd_una to th->th_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:

		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			if (tlen == 0 && !dupseg && tiwin == tp->snd_wnd) {
				TCP_STATINC(TCP_STAT_RCVDUPACK);
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change), the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 */
				if (TCP_TIMER_ISARMED(tp, TCPT_REXMT) == 0 ||
				    th->th_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (tp->t_partialacks < 0 &&
					 (++tp->t_dupacks == tcprexmtthresh ||
					 TCP_FACK_FASTRECOV(tp))) {
					/*
					 * Do the fast retransmit, and adjust
					 * congestion control paramenters.
					 */
					if (tp->t_congctl->fast_retransmit(tp, th)) {
						/* False fast retransmit */
						break;
					} else
						goto drop;
				} else if (tp->t_dupacks > tcprexmtthresh) {
					tp->snd_cwnd += tp->t_segsz;
					KERNEL_LOCK(1, NULL);
					(void) tcp_output(tp);
					KERNEL_UNLOCK_ONE(NULL);
					goto drop;
				}
			} else {
				/*
				 * If the ack appears to be very old, only
				 * allow data that is in-sequence.  This
				 * makes it somewhat more difficult to insert
				 * forged data by guessing sequence numbers.
				 * Sent an ack to try to update the send
				 * sequence number on the other side.
				 */
				if (tlen && th->th_seq != tp->rcv_nxt &&
				    SEQ_LT(th->th_ack,
				    tp->snd_una - tp->max_sndwnd))
					goto dropafterack;
			}
			break;
		}
		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		tp->t_congctl->fast_retransmit_newack(tp, th);

		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			TCP_STATINC(TCP_STAT_RCVACKTOOMUCH);
			goto dropafterack;
		}
		acked = th->th_ack - tp->snd_una;
		tcps = TCP_STAT_GETREF();
		tcps[TCP_STAT_RCVACKPACK]++;
		tcps[TCP_STAT_RCVACKBYTE] += acked;
		TCP_STAT_PUTREF();

		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 */
		if (ts_rtt)
			tcp_xmit_timer(tp, ts_rtt - 1);
		else if (tp->t_rtttime && SEQ_GT(th->th_ack, tp->t_rtseq))
			tcp_xmit_timer(tp, tcp_now - tp->t_rtttime);

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			TCP_TIMER_DISARM(tp, TCPT_REXMT);
			needoutput = 1;
		} else if (TCP_TIMER_ISARMED(tp, TCPT_PERSIST) == 0)
			TCP_TIMER_ARM(tp, TCPT_REXMT, tp->t_rxtcur);

		/*
		 * New data has been acked, adjust the congestion window.
		 */
		tp->t_congctl->newack(tp, th);

		nd6_hint(tp);
		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop(&so->so_snd, (int)so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			if (acked > (tp->t_lastoff - tp->t_inoff))
				tp->t_lastm = NULL;
			sbdrop(&so->so_snd, acked);
			tp->t_lastoff -= acked;
			if (tp->snd_wnd > acked)
				tp->snd_wnd -= acked;
			else
				tp->snd_wnd = 0;
			ourfinisacked = 0;
		}
		sowwakeup(so);

		icmp_check(tp, th, acked);

		tp->snd_una = th->th_ack;
		if (SEQ_GT(tp->snd_una, tp->snd_fack))
			tp->snd_fack = tp->snd_una;
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;
		if (SEQ_LT(tp->snd_high, tp->snd_una))
			tp->snd_high = tp->snd_una;

		switch (tp->t_state) {

		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				/*
				 * If we can't receive any more
				 * data, then closing user can proceed.
				 * Starting the timer is contrary to the
				 * specification, but if we don't get a FIN
				 * we'll hang forever.
				 */
				if (so->so_state & SS_CANTRCVMORE) {
					soisdisconnected(so);
					if (tp->t_maxidle > 0)
						TCP_TIMER_ARM(tp, TCPT_2MSL,
						    tp->t_maxidle);
				}
				tp->t_state = TCPS_FIN_WAIT_2;
			}
			break;

	 	/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
				tp->t_state = TCPS_TIME_WAIT;
				tcp_canceltimers(tp);
				TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * tp->t_msl);
				soisdisconnected(so);
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				tp = tcp_close(tp);
				goto drop;
			}
			break;

		/*
		 * In TIME_WAIT state the only thing that should arrive
		 * is a retransmission of the remote FIN.  Acknowledge
		 * it and restart the finack timer.
		 */
		case TCPS_TIME_WAIT:
			TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * tp->t_msl);
			goto dropafterack;
		}
	}

step6:
	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((tiflags & TH_ACK) && (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
	    (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tlen == 0 &&
		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
			TCP_STATINC(TCP_STAT_RCVWINUPD);
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((tiflags & TH_URG) && th->th_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * This is a kludge, but if we receive and accept
		 * random urgent pointers, we'll crash in
		 * soreceive.  It's hard to imagine someone
		 * actually wanting to send this much urgent data.
		 */
		if (th->th_urp + so->so_rcv.sb_cc > sb_max) {
			th->th_urp = 0;			/* XXX */
			tiflags &= ~TH_URG;		/* XXX */
			goto dodata;			/* XXX */
		}
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side.
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(th->th_seq+th->th_urp, tp->rcv_up)) {
			tp->rcv_up = th->th_seq + th->th_urp;
			so->so_oobmark = so->so_rcv.sb_cc +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0)
				so->so_state |= SS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (th->th_urp <= (u_int16_t) tlen
#ifdef SO_OOBINLINE
		     && (so->so_options & SO_OOBINLINE) == 0
#endif
		     )
			tcp_pulloutofband(so, th, m, hdroptlen);
	} else
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
dodata:							/* XXX */

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgement of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * tcp_rcvd()).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((tlen || (tiflags & TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * Insert segment ti into reassembly queue of tcp with
		 * control block tp.  Return TH_FIN if reassembly now includes
		 * a segment with FIN.  The macro form does the common case
		 * inline (segment is the next to be received on an
		 * established connection, and the queue is empty),
		 * avoiding linkage into and removal from the queue and
		 * repetition of various conversions.
		 * Set DELACK for segments received in order, but ack
		 * immediately when segments are out of order
		 * (so fast retransmit can work).
		 */
		/* NOTE: this was TCP_REASS() macro, but used only once */
		TCP_REASS_LOCK(tp);
		if (th->th_seq == tp->rcv_nxt &&
		    TAILQ_FIRST(&tp->segq) == NULL &&
		    tp->t_state == TCPS_ESTABLISHED) {
			tcp_setup_ack(tp, th);
			tp->rcv_nxt += tlen;
			tiflags = th->th_flags & TH_FIN;
			tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_RCVPACK]++;
			tcps[TCP_STAT_RCVBYTE] += tlen;
			TCP_STAT_PUTREF();
			nd6_hint(tp);
			if (so->so_state & SS_CANTRCVMORE)
				m_freem(m);
			else {
				m_adj(m, hdroptlen);
				sbappendstream(&(so)->so_rcv, m);
			}
			TCP_REASS_UNLOCK(tp);
			sorwakeup(so);
		} else {
			m_adj(m, hdroptlen);
			tiflags = tcp_reass(tp, th, m, &tlen);
			tp->t_flags |= TF_ACKNOW;
		}

		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 */
		len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
	} else {
		m_freem(m);
		m = NULL;
		tiflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.  Ignore a FIN received before
	 * the connection is fully established.
	 */
	if ((tiflags & TH_FIN) && TCPS_HAVEESTABLISHED(tp->t_state)) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {

	 	/*
		 * In ESTABLISHED STATE enter the CLOSE_WAIT state.
		 */
		case TCPS_ESTABLISHED:
			tp->t_state = TCPS_CLOSE_WAIT;
			break;

	 	/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tp->t_state = TCPS_CLOSING;
			break;

	 	/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			tp->t_state = TCPS_TIME_WAIT;
			tcp_canceltimers(tp);
			TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * tp->t_msl);
			soisdisconnected(so);
			break;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * tp->t_msl);
			break;
		}
	}
#ifdef TCP_DEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, tcp_saveti, 0);
#endif

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW)) {
		KERNEL_LOCK(1, NULL);
		(void) tcp_output(tp);
		KERNEL_UNLOCK_ONE(NULL);
	}
	if (tcp_saveti)
		m_freem(tcp_saveti);

	if (tp->t_state == TCPS_TIME_WAIT
	    && (so->so_state & SS_NOFDREF)
	    && (tp->t_inpcb || af != AF_INET)
	    && (tp->t_in6pcb || af != AF_INET6)
	    && ((af == AF_INET ? tcp4_vtw_enable : tcp6_vtw_enable) & 1) != 0
	    && TAILQ_EMPTY(&tp->segq)
	    && vtw_add(af, tp)) {
		;
	}
	return;

badsyn:
	/*
	 * Received a bad SYN.  Increment counters and dropwithreset.
	 */
	TCP_STATINC(TCP_STAT_BADSYN);
	tp = NULL;
	goto dropwithreset;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 */
	if (tiflags & TH_RST)
		goto drop;
	goto dropafterack2;

dropafterack_ratelim:
	/*
	 * We may want to rate-limit ACKs against SYN/RST attack.
	 */
	if (ppsratecheck(&tcp_ackdrop_ppslim_last, &tcp_ackdrop_ppslim_count,
	    tcp_ackdrop_ppslim) == 0) {
		/* XXX stat */
		goto drop;
	}
	/* ...fall into dropafterack2... */

dropafterack2:
	m_freem(m);
	tp->t_flags |= TF_ACKNOW;
	KERNEL_LOCK(1, NULL);
	(void) tcp_output(tp);
	KERNEL_UNLOCK_ONE(NULL);
	if (tcp_saveti)
		m_freem(tcp_saveti);
	return;

dropwithreset_ratelim:
	/*
	 * We may want to rate-limit RSTs in certain situations,
	 * particularly if we are sending an RST in response to
	 * an attempt to connect to or otherwise communicate with
	 * a port for which we have no socket.
	 */
	if (ppsratecheck(&tcp_rst_ppslim_last, &tcp_rst_ppslim_count,
	    tcp_rst_ppslim) == 0) {
		/* XXX stat */
		goto drop;
	}
	/* ...fall into dropwithreset... */

dropwithreset:
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 */
	if (tiflags & TH_RST)
		goto drop;

	switch (af) {
#ifdef INET6
	case AF_INET6:
		/* For following calls to tcp_respond */
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst))
			goto drop;
		break;
#endif /* INET6 */
	case AF_INET:
		if (IN_MULTICAST(ip->ip_dst.s_addr) ||
		    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
			goto drop;
	}

	if (tiflags & TH_ACK)
		(void)tcp_respond(tp, m, m, th, (tcp_seq)0, th->th_ack, TH_RST);
	else {
		if (tiflags & TH_SYN)
			tlen++;
		(void)tcp_respond(tp, m, m, th, th->th_seq + tlen, (tcp_seq)0,
		    TH_RST|TH_ACK);
	}
	if (tcp_saveti)
		m_freem(tcp_saveti);
	return;

badcsum:
drop:
	/*
	 * Drop space held by incoming segment and return.
	 */
	if (tp) {
		if (tp->t_inpcb)
			so = tp->t_inpcb->inp_socket;
#ifdef INET6
		else if (tp->t_in6pcb)
			so = tp->t_in6pcb->in6p_socket;
#endif
		else
			so = NULL;
#ifdef TCP_DEBUG
		if (so && (so->so_options & SO_DEBUG) != 0)
			tcp_trace(TA_DROP, ostate, tp, tcp_saveti, 0);
#endif
	}
	if (tcp_saveti)
		m_freem(tcp_saveti);
	m_freem(m);
	return;
}

#ifdef TCP_SIGNATURE
int
tcp_signature_apply(void *fstate, void *data, u_int len)
{

	MD5Update(fstate, (u_char *)data, len);
	return (0);
}

struct secasvar *
tcp_signature_getsav(struct mbuf *m, struct tcphdr *th)
{
	struct ip *ip;
	struct ip6_hdr *ip6;

	ip = mtod(m, struct ip *);
	switch (ip->ip_v) {
	case 4:
		ip = mtod(m, struct ip *);
		ip6 = NULL;
		break;
	case 6:
		ip = NULL;
		ip6 = mtod(m, struct ip6_hdr *);
		break;
	default:
		return (NULL);
	}

#ifdef IPSEC
	if (ipsec_used) {
		union sockaddr_union dst;
		/* Extract the destination from the IP header in the mbuf. */
		memset(&dst, 0, sizeof(union sockaddr_union));
		if (ip != NULL) {
			dst.sa.sa_len = sizeof(struct sockaddr_in);
			dst.sa.sa_family = AF_INET;
			dst.sin.sin_addr = ip->ip_dst;
		} else {
			dst.sa.sa_len = sizeof(struct sockaddr_in6);
			dst.sa.sa_family = AF_INET6;
			dst.sin6.sin6_addr = ip6->ip6_dst;
		}

		/*
		 * Look up an SADB entry which matches the address of the peer.
		 */
		return KEY_ALLOCSA(&dst, IPPROTO_TCP, htonl(TCP_SIG_SPI), 0, 0);
	}
	return NULL;
#else
	if (ip)
		return key_allocsa(AF_INET, (void *)&ip->ip_src,
		    (void *)&ip->ip_dst, IPPROTO_TCP,
		    htonl(TCP_SIG_SPI), 0, 0);
	else
		return key_allocsa(AF_INET6, (void *)&ip6->ip6_src,
		    (void *)&ip6->ip6_dst, IPPROTO_TCP,
		    htonl(TCP_SIG_SPI), 0, 0);
#endif
}

int
tcp_signature(struct mbuf *m, struct tcphdr *th, int thoff,
    struct secasvar *sav, char *sig)
{
	MD5_CTX ctx;
	struct ip *ip;
	struct ipovly *ipovly;
#ifdef INET6
	struct ip6_hdr *ip6;
	struct ip6_hdr_pseudo ip6pseudo;
#endif /* INET6 */
	struct ippseudo ippseudo;
	struct tcphdr th0;
	int l, tcphdrlen;

	if (sav == NULL)
		return (-1);

	tcphdrlen = th->th_off * 4;

	switch (mtod(m, struct ip *)->ip_v) {
	case 4:
		MD5Init(&ctx);
		ip = mtod(m, struct ip *);
		memset(&ippseudo, 0, sizeof(ippseudo));
		ipovly = (struct ipovly *)ip;
		ippseudo.ippseudo_src = ipovly->ih_src;
		ippseudo.ippseudo_dst = ipovly->ih_dst;
		ippseudo.ippseudo_pad = 0;
		ippseudo.ippseudo_p = IPPROTO_TCP;
		ippseudo.ippseudo_len = htons(m->m_pkthdr.len - thoff);
		MD5Update(&ctx, (char *)&ippseudo, sizeof(ippseudo));
		break;
#if INET6
	case 6:
		MD5Init(&ctx);
		ip6 = mtod(m, struct ip6_hdr *);
		memset(&ip6pseudo, 0, sizeof(ip6pseudo));
		ip6pseudo.ip6ph_src = ip6->ip6_src;
		in6_clearscope(&ip6pseudo.ip6ph_src);
		ip6pseudo.ip6ph_dst = ip6->ip6_dst;
		in6_clearscope(&ip6pseudo.ip6ph_dst);
		ip6pseudo.ip6ph_len = htons(m->m_pkthdr.len - thoff);
		ip6pseudo.ip6ph_nxt = IPPROTO_TCP;
		MD5Update(&ctx, (char *)&ip6pseudo, sizeof(ip6pseudo));
		break;
#endif /* INET6 */
	default:
		return (-1);
	}

	th0 = *th;
	th0.th_sum = 0;
	MD5Update(&ctx, (char *)&th0, sizeof(th0));

	l = m->m_pkthdr.len - thoff - tcphdrlen;
	if (l > 0)
		m_apply(m, thoff + tcphdrlen,
		    m->m_pkthdr.len - thoff - tcphdrlen,
		    tcp_signature_apply, &ctx);

	MD5Update(&ctx, _KEYBUF(sav->key_auth), _KEYLEN(sav->key_auth));
	MD5Final(sig, &ctx);

	return (0);
}
#endif

/*
 * tcp_dooptions: parse and process tcp options.
 *
 * returns -1 if this segment should be dropped.  (eg. wrong signature)
 * otherwise returns 0.
 */

static int
tcp_dooptions(struct tcpcb *tp, const u_char *cp, int cnt,
    struct tcphdr *th,
    struct mbuf *m, int toff, struct tcp_opt_info *oi)
{
	u_int16_t mss;
	int opt, optlen = 0;
#ifdef TCP_SIGNATURE
	void *sigp = NULL;
	char sigbuf[TCP_SIGLEN];
	struct secasvar *sav = NULL;
#endif

	for (; cp && cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		switch (opt) {

		default:
			continue;

		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG)
				continue;
			if (!(th->th_flags & TH_SYN))
				continue;
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			bcopy(cp + 2, &mss, sizeof(mss));
			oi->maxseg = ntohs(mss);
			break;

		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (!(th->th_flags & TH_SYN))
				continue;
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			tp->t_flags |= TF_RCVD_SCALE;
			tp->requested_s_scale = cp[2];
			if (tp->requested_s_scale > TCP_MAX_WINSHIFT) {
				char buf[INET6_ADDRSTRLEN];
				struct ip *ip = mtod(m, struct ip *);
#ifdef INET6
				struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
#endif
				if (ip)
					in_print(buf, sizeof(buf),
					    &ip->ip_src);
#ifdef INET6
				else if (ip6)
					in6_print(buf, sizeof(buf),
					    &ip6->ip6_src);
#endif
				else
					strlcpy(buf, "(unknown)", sizeof(buf));
				log(LOG_ERR, "TCP: invalid wscale %d from %s, "
				    "assuming %d\n",
				    tp->requested_s_scale, buf,
				    TCP_MAX_WINSHIFT);
				tp->requested_s_scale = TCP_MAX_WINSHIFT;
			}
			break;

		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			oi->ts_present = 1;
			bcopy(cp + 2, &oi->ts_val, sizeof(oi->ts_val));
			NTOHL(oi->ts_val);
			bcopy(cp + 6, &oi->ts_ecr, sizeof(oi->ts_ecr));
			NTOHL(oi->ts_ecr);

			if (!(th->th_flags & TH_SYN))
				continue;
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			/*
			 * A timestamp received in a SYN makes
			 * it ok to send timestamp requests and replies.
			 */
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = oi->ts_val;
			tp->ts_recent_age = tcp_now;
                        break;

		case TCPOPT_SACK_PERMITTED:
			if (optlen != TCPOLEN_SACK_PERMITTED)
				continue;
			if (!(th->th_flags & TH_SYN))
				continue;
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			if (tcp_do_sack) {
				tp->t_flags |= TF_SACK_PERMIT;
				tp->t_flags |= TF_WILL_SACK;
			}
			break;

		case TCPOPT_SACK:
			tcp_sack_option(tp, th, cp, optlen);
			break;
#ifdef TCP_SIGNATURE
		case TCPOPT_SIGNATURE:
			if (optlen != TCPOLEN_SIGNATURE)
				continue;
			if (sigp && memcmp(sigp, cp + 2, TCP_SIGLEN))
				return (-1);

			sigp = sigbuf;
			memcpy(sigbuf, cp + 2, TCP_SIGLEN);
			tp->t_flags |= TF_SIGNATURE;
			break;
#endif
		}
	}

#ifndef TCP_SIGNATURE
	return 0;
#else
	if (tp->t_flags & TF_SIGNATURE) {

		sav = tcp_signature_getsav(m, th);

		if (sav == NULL && tp->t_state == TCPS_LISTEN)
			return (-1);
	}

	if ((sigp ? TF_SIGNATURE : 0) ^ (tp->t_flags & TF_SIGNATURE))
		goto out;

	if (sigp) {
		char sig[TCP_SIGLEN];

		tcp_fields_to_net(th);
		if (tcp_signature(m, th, toff, sav, sig) < 0) {
			tcp_fields_to_host(th);
			goto out;
		}
		tcp_fields_to_host(th);

		if (memcmp(sig, sigp, TCP_SIGLEN)) {
			TCP_STATINC(TCP_STAT_BADSIG);
			goto out;
		} else
			TCP_STATINC(TCP_STAT_GOODSIG);

		key_sa_recordxfer(sav, m);
		KEY_FREESAV(&sav);
	}
	return 0;
out:
	if (sav != NULL)
		KEY_FREESAV(&sav);
	return -1;
#endif
}

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
void
tcp_pulloutofband(struct socket *so, struct tcphdr *th,
    struct mbuf *m, int off)
{
	int cnt = off + th->th_urp - 1;

	while (cnt >= 0) {
		if (m->m_len > cnt) {
			char *cp = mtod(m, char *) + cnt;
			struct tcpcb *tp = sototcpcb(so);

			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp+1, cp, (unsigned)(m->m_len - cnt - 1));
			m->m_len--;
			return;
		}
		cnt -= m->m_len;
		m = m->m_next;
		if (m == 0)
			break;
	}
	panic("tcp_pulloutofband");
}

/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 *
 * rtt is in units of slow ticks (typically 500 ms) -- essentially the
 * difference of two timestamps.
 */
void
tcp_xmit_timer(struct tcpcb *tp, uint32_t rtt)
{
	int32_t delta;

	TCP_STATINC(TCP_STAT_RTTUPDATED);
	if (tp->t_srtt != 0) {
		/*
		 * Compute the amount to add to srtt for smoothing,
		 * *alpha, or 2^(-TCP_RTT_SHIFT).  Because
		 * srtt is stored in 1/32 slow ticks, we conceptually
		 * shift left 5 bits, subtract srtt to get the
		 * diference, and then shift right by TCP_RTT_SHIFT
		 * (3) to obtain 1/8 of the difference.
		 */
		delta = (rtt << 2) - (tp->t_srtt >> TCP_RTT_SHIFT);
		/* 
		 * This can never happen, because delta's lowest
		 * possible value is 1/8 of t_srtt.  But if it does,
		 * set srtt to some reasonable value, here chosen
		 * as 1/8 tick.
		 */
		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1 << 2;
		/*
		 * RFC2988 requires that rttvar be updated first.
		 * This code is compliant because "delta" is the old
		 * srtt minus the new observation (scaled).
		 *
		 * RFC2988 says:
		 *   rttvar = (1-beta) * rttvar + beta * |srtt-observed|
		 *
		 * delta is in units of 1/32 ticks, and has then been
		 * divided by 8.  This is equivalent to being in 1/16s
		 * units and divided by 4.  Subtract from it 1/4 of
		 * the existing rttvar to form the (signed) amount to
		 * adjust.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
		/*
		 * As with srtt, this should never happen.  There is
		 * no support in RFC2988 for this operation.  But 1/4s
		 * as rttvar when faced with something arguably wrong
		 * is ok.
		 */
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1 << 2;

		/*
		 * If srtt exceeds .01 second, ensure we use the 'remote' MSL
		 * Problem is: it doesn't work.  Disabled by defaulting
		 * tcp_rttlocal to 0; see corresponding code in
		 * tcp_subr that selects local vs remote in a different way.
		 *
		 * The static branch prediction hint here should be removed
		 * when the rtt estimator is fixed and the rtt_enable code
		 * is turned back on.
		 */
		if (__predict_false(tcp_rttlocal) && tcp_msl_enable
		    && tp->t_srtt > tcp_msl_remote_threshold
		    && tp->t_msl  < tcp_msl_remote) {
			tp->t_msl = tcp_msl_remote;
		}
	} else {
		/*
		 * This is the first measurement.  Per RFC2988, 2.2,
		 * set rtt=R and srtt=R/2.
		 * For srtt, storage representation is 1/32 ticks,
		 * so shift left by 5.
		 * For rttvar, storage representation is 1/16 ticks,
		 * So shift left by 4, but then right by 1 to halve.
		 */
		tp->t_srtt = rtt << (TCP_RTT_SHIFT + 2);
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT + 2 - 1);
	}
	tp->t_rtttime = 0;
	tp->t_rxtshift = 0;

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
	    max(tp->t_rttmin, rtt + 2), TCPTV_REXMTMAX);

	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;
}


/*
 * TCP compressed state engine.  Currently used to hold compressed
 * state for SYN_RECEIVED.
 */

u_long	syn_cache_count;
u_int32_t syn_hash1, syn_hash2;

#define SYN_HASH(sa, sp, dp) \
	((((sa)->s_addr^syn_hash1)*(((((u_int32_t)(dp))<<16) + \
				     ((u_int32_t)(sp)))^syn_hash2)))
#ifndef INET6
#define	SYN_HASHALL(hash, src, dst) \
do {									\
	hash = SYN_HASH(&((const struct sockaddr_in *)(src))->sin_addr,	\
		((const struct sockaddr_in *)(src))->sin_port,		\
		((const struct sockaddr_in *)(dst))->sin_port);		\
} while (/*CONSTCOND*/ 0)
#else
#define SYN_HASH6(sa, sp, dp) \
	((((sa)->s6_addr32[0] ^ (sa)->s6_addr32[3] ^ syn_hash1) * \
	  (((((u_int32_t)(dp))<<16) + ((u_int32_t)(sp)))^syn_hash2)) \
	 & 0x7fffffff)

#define SYN_HASHALL(hash, src, dst) \
do {									\
	switch ((src)->sa_family) {					\
	case AF_INET:							\
		hash = SYN_HASH(&((const struct sockaddr_in *)(src))->sin_addr, \
			((const struct sockaddr_in *)(src))->sin_port,	\
			((const struct sockaddr_in *)(dst))->sin_port);	\
		break;							\
	case AF_INET6:							\
		hash = SYN_HASH6(&((const struct sockaddr_in6 *)(src))->sin6_addr, \
			((const struct sockaddr_in6 *)(src))->sin6_port,	\
			((const struct sockaddr_in6 *)(dst))->sin6_port);	\
		break;							\
	default:							\
		hash = 0;						\
	}								\
} while (/*CONSTCOND*/0)
#endif /* INET6 */

static struct pool syn_cache_pool;

/*
 * We don't estimate RTT with SYNs, so each packet starts with the default
 * RTT and each timer step has a fixed timeout value.
 */
#define	SYN_CACHE_TIMER_ARM(sc)						\
do {									\
	TCPT_RANGESET((sc)->sc_rxtcur,					\
	    TCPTV_SRTTDFLT * tcp_backoff[(sc)->sc_rxtshift], TCPTV_MIN,	\
	    TCPTV_REXMTMAX);						\
	callout_reset(&(sc)->sc_timer,					\
	    (sc)->sc_rxtcur * (hz / PR_SLOWHZ), syn_cache_timer, (sc));	\
} while (/*CONSTCOND*/0)

#define	SYN_CACHE_TIMESTAMP(sc)	(tcp_now - (sc)->sc_timebase)

static inline void
syn_cache_rm(struct syn_cache *sc)
{
	TAILQ_REMOVE(&tcp_syn_cache[sc->sc_bucketidx].sch_bucket,
	    sc, sc_bucketq);
	sc->sc_tp = NULL;
	LIST_REMOVE(sc, sc_tpq);
	tcp_syn_cache[sc->sc_bucketidx].sch_length--;
	callout_stop(&sc->sc_timer);
	syn_cache_count--;
}

static inline void
syn_cache_put(struct syn_cache *sc)
{
	if (sc->sc_ipopts)
		(void) m_free(sc->sc_ipopts);
	rtcache_free(&sc->sc_route);
	sc->sc_flags |= SCF_DEAD;
	if (!callout_invoking(&sc->sc_timer))
		callout_schedule(&(sc)->sc_timer, 1);
}

void
syn_cache_init(void)
{
	int i;

	pool_init(&syn_cache_pool, sizeof(struct syn_cache), 0, 0, 0,
	    "synpl", NULL, IPL_SOFTNET);

	/* Initialize the hash buckets. */
	for (i = 0; i < tcp_syn_cache_size; i++)
		TAILQ_INIT(&tcp_syn_cache[i].sch_bucket);
}

void
syn_cache_insert(struct syn_cache *sc, struct tcpcb *tp)
{
	struct syn_cache_head *scp;
	struct syn_cache *sc2;
	int s;

	/*
	 * If there are no entries in the hash table, reinitialize
	 * the hash secrets.
	 */
	if (syn_cache_count == 0) {
		syn_hash1 = cprng_fast32();
		syn_hash2 = cprng_fast32();
	}

	SYN_HASHALL(sc->sc_hash, &sc->sc_src.sa, &sc->sc_dst.sa);
	sc->sc_bucketidx = sc->sc_hash % tcp_syn_cache_size;
	scp = &tcp_syn_cache[sc->sc_bucketidx];

	/*
	 * Make sure that we don't overflow the per-bucket
	 * limit or the total cache size limit.
	 */
	s = splsoftnet();
	if (scp->sch_length >= tcp_syn_bucket_limit) {
		TCP_STATINC(TCP_STAT_SC_BUCKETOVERFLOW);
		/*
		 * The bucket is full.  Toss the oldest element in the
		 * bucket.  This will be the first entry in the bucket.
		 */
		sc2 = TAILQ_FIRST(&scp->sch_bucket);
#ifdef DIAGNOSTIC
		/*
		 * This should never happen; we should always find an
		 * entry in our bucket.
		 */
		if (sc2 == NULL)
			panic("syn_cache_insert: bucketoverflow: impossible");
#endif
		syn_cache_rm(sc2);
		syn_cache_put(sc2);	/* calls pool_put but see spl above */
	} else if (syn_cache_count >= tcp_syn_cache_limit) {
		struct syn_cache_head *scp2, *sce;

		TCP_STATINC(TCP_STAT_SC_OVERFLOWED);
		/*
		 * The cache is full.  Toss the oldest entry in the
		 * first non-empty bucket we can find.
		 *
		 * XXX We would really like to toss the oldest
		 * entry in the cache, but we hope that this
		 * condition doesn't happen very often.
		 */
		scp2 = scp;
		if (TAILQ_EMPTY(&scp2->sch_bucket)) {
			sce = &tcp_syn_cache[tcp_syn_cache_size];
			for (++scp2; scp2 != scp; scp2++) {
				if (scp2 >= sce)
					scp2 = &tcp_syn_cache[0];
				if (! TAILQ_EMPTY(&scp2->sch_bucket))
					break;
			}
#ifdef DIAGNOSTIC
			/*
			 * This should never happen; we should always find a
			 * non-empty bucket.
			 */
			if (scp2 == scp)
				panic("syn_cache_insert: cacheoverflow: "
				    "impossible");
#endif
		}
		sc2 = TAILQ_FIRST(&scp2->sch_bucket);
		syn_cache_rm(sc2);
		syn_cache_put(sc2);	/* calls pool_put but see spl above */
	}

	/*
	 * Initialize the entry's timer.
	 */
	sc->sc_rxttot = 0;
	sc->sc_rxtshift = 0;
	SYN_CACHE_TIMER_ARM(sc);

	/* Link it from tcpcb entry */
	LIST_INSERT_HEAD(&tp->t_sc, sc, sc_tpq);

	/* Put it into the bucket. */
	TAILQ_INSERT_TAIL(&scp->sch_bucket, sc, sc_bucketq);
	scp->sch_length++;
	syn_cache_count++;

	TCP_STATINC(TCP_STAT_SC_ADDED);
	splx(s);
}

/*
 * Walk the timer queues, looking for SYN,ACKs that need to be retransmitted.
 * If we have retransmitted an entry the maximum number of times, expire
 * that entry.
 */
void
syn_cache_timer(void *arg)
{
	struct syn_cache *sc = arg;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);
	callout_ack(&sc->sc_timer);

	if (__predict_false(sc->sc_flags & SCF_DEAD)) {
		TCP_STATINC(TCP_STAT_SC_DELAYED_FREE);
		callout_destroy(&sc->sc_timer);
		pool_put(&syn_cache_pool, sc);
		KERNEL_UNLOCK_ONE(NULL);
		mutex_exit(softnet_lock);
		return;
	}

	if (__predict_false(sc->sc_rxtshift == TCP_MAXRXTSHIFT)) {
		/* Drop it -- too many retransmissions. */
		goto dropit;
	}

	/*
	 * Compute the total amount of time this entry has
	 * been on a queue.  If this entry has been on longer
	 * than the keep alive timer would allow, expire it.
	 */
	sc->sc_rxttot += sc->sc_rxtcur;
	if (sc->sc_rxttot >= tcp_keepinit)
		goto dropit;

	TCP_STATINC(TCP_STAT_SC_RETRANSMITTED);
	(void) syn_cache_respond(sc, NULL);

	/* Advance the timer back-off. */
	sc->sc_rxtshift++;
	SYN_CACHE_TIMER_ARM(sc);

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
	return;

 dropit:
	TCP_STATINC(TCP_STAT_SC_TIMED_OUT);
	syn_cache_rm(sc);
	if (sc->sc_ipopts)
		(void) m_free(sc->sc_ipopts);
	rtcache_free(&sc->sc_route);
	callout_destroy(&sc->sc_timer);
	pool_put(&syn_cache_pool, sc);
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * Remove syn cache created by the specified tcb entry,
 * because this does not make sense to keep them
 * (if there's no tcb entry, syn cache entry will never be used)
 */
void
syn_cache_cleanup(struct tcpcb *tp)
{
	struct syn_cache *sc, *nsc;
	int s;

	s = splsoftnet();

	for (sc = LIST_FIRST(&tp->t_sc); sc != NULL; sc = nsc) {
		nsc = LIST_NEXT(sc, sc_tpq);

#ifdef DIAGNOSTIC
		if (sc->sc_tp != tp)
			panic("invalid sc_tp in syn_cache_cleanup");
#endif
		syn_cache_rm(sc);
		syn_cache_put(sc);	/* calls pool_put but see spl above */
	}
	/* just for safety */
	LIST_INIT(&tp->t_sc);

	splx(s);
}

/*
 * Find an entry in the syn cache.
 */
struct syn_cache *
syn_cache_lookup(const struct sockaddr *src, const struct sockaddr *dst,
    struct syn_cache_head **headp)
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	u_int32_t hash;
	int s;

	SYN_HASHALL(hash, src, dst);

	scp = &tcp_syn_cache[hash % tcp_syn_cache_size];
	*headp = scp;
	s = splsoftnet();
	for (sc = TAILQ_FIRST(&scp->sch_bucket); sc != NULL;
	     sc = TAILQ_NEXT(sc, sc_bucketq)) {
		if (sc->sc_hash != hash)
			continue;
		if (!memcmp(&sc->sc_src, src, src->sa_len) &&
		    !memcmp(&sc->sc_dst, dst, dst->sa_len)) {
			splx(s);
			return (sc);
		}
	}
	splx(s);
	return (NULL);
}

/*
 * This function gets called when we receive an ACK for a
 * socket in the LISTEN state.  We look up the connection
 * in the syn cache, and if its there, we pull it out of
 * the cache and turn it into a full-blown connection in
 * the SYN-RECEIVED state.
 *
 * The return values may not be immediately obvious, and their effects
 * can be subtle, so here they are:
 *
 *	NULL	SYN was not found in cache; caller should drop the
 *		packet and send an RST.
 *
 *	-1	We were unable to create the new connection, and are
 *		aborting it.  An ACK,RST is being sent to the peer
 *		(unless we got screwey sequence numbners; see below),
 *		because the 3-way handshake has been completed.  Caller
 *		should not free the mbuf, since we may be using it.  If
 *		we are not, we will free it.
 *
 *	Otherwise, the return value is a pointer to the new socket
 *	associated with the connection.
 */
struct socket *
syn_cache_get(struct sockaddr *src, struct sockaddr *dst,
    struct tcphdr *th, unsigned int hlen, unsigned int tlen,
    struct socket *so, struct mbuf *m)
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	struct inpcb *inp = NULL;
#ifdef INET6
	struct in6pcb *in6p = NULL;
#endif
	struct tcpcb *tp = 0;
	int s;
	struct socket *oso;

	s = splsoftnet();
	if ((sc = syn_cache_lookup(src, dst, &scp)) == NULL) {
		splx(s);
		return (NULL);
	}

	/*
	 * Verify the sequence and ack numbers.  Try getting the correct
	 * response again.
	 */
	if ((th->th_ack != sc->sc_iss + 1) ||
	    SEQ_LEQ(th->th_seq, sc->sc_irs) ||
	    SEQ_GT(th->th_seq, sc->sc_irs + 1 + sc->sc_win)) {
		(void) syn_cache_respond(sc, m);
		splx(s);
		return ((struct socket *)(-1));
	}

	/* Remove this cache entry */
	syn_cache_rm(sc);
	splx(s);

	/*
	 * Ok, create the full blown connection, and set things up
	 * as they would have been set up if we had created the
	 * connection when the SYN arrived.  If we can't create
	 * the connection, abort it.
	 */
	/*
	 * inp still has the OLD in_pcb stuff, set the
	 * v6-related flags on the new guy, too.   This is
	 * done particularly for the case where an AF_INET6
	 * socket is bound only to a port, and a v4 connection
	 * comes in on that port.
	 * we also copy the flowinfo from the original pcb
	 * to the new one.
	 */
	oso = so;
	so = sonewconn(so, true);
	if (so == NULL)
		goto resetandabort;

	switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET
	case AF_INET:
		inp = sotoinpcb(so);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		in6p = sotoin6pcb(so);
		break;
#endif
	}
	switch (src->sa_family) {
#ifdef INET
	case AF_INET:
		if (inp) {
			inp->inp_laddr = ((struct sockaddr_in *)dst)->sin_addr;
			inp->inp_lport = ((struct sockaddr_in *)dst)->sin_port;
			inp->inp_options = ip_srcroute();
			in_pcbstate(inp, INP_BOUND);
			if (inp->inp_options == NULL) {
				inp->inp_options = sc->sc_ipopts;
				sc->sc_ipopts = NULL;
			}
		}
#ifdef INET6
		else if (in6p) {
			/* IPv4 packet to AF_INET6 socket */
			memset(&in6p->in6p_laddr, 0, sizeof(in6p->in6p_laddr));
			in6p->in6p_laddr.s6_addr16[5] = htons(0xffff);
			bcopy(&((struct sockaddr_in *)dst)->sin_addr,
				&in6p->in6p_laddr.s6_addr32[3],
				sizeof(((struct sockaddr_in *)dst)->sin_addr));
			in6p->in6p_lport = ((struct sockaddr_in *)dst)->sin_port;
			in6totcpcb(in6p)->t_family = AF_INET;
			if (sotoin6pcb(oso)->in6p_flags & IN6P_IPV6_V6ONLY)
				in6p->in6p_flags |= IN6P_IPV6_V6ONLY;
			else
				in6p->in6p_flags &= ~IN6P_IPV6_V6ONLY;
			in6_pcbstate(in6p, IN6P_BOUND);
		}
#endif
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (in6p) {
			in6p->in6p_laddr = ((struct sockaddr_in6 *)dst)->sin6_addr;
			in6p->in6p_lport = ((struct sockaddr_in6 *)dst)->sin6_port;
			in6_pcbstate(in6p, IN6P_BOUND);
		}
		break;
#endif
	}
#ifdef INET6
	if (in6p && in6totcpcb(in6p)->t_family == AF_INET6 && sotoinpcb(oso)) {
		struct in6pcb *oin6p = sotoin6pcb(oso);
		/* inherit socket options from the listening socket */
		in6p->in6p_flags |= (oin6p->in6p_flags & IN6P_CONTROLOPTS);
		if (in6p->in6p_flags & IN6P_CONTROLOPTS) {
			m_freem(in6p->in6p_options);
			in6p->in6p_options = 0;
		}
		ip6_savecontrol(in6p, &in6p->in6p_options,
			mtod(m, struct ip6_hdr *), m);
	}
#endif

#if defined(IPSEC)
	if (ipsec_used) {
		/*
		 * we make a copy of policy, instead of sharing the policy, for
		 * better behavior in terms of SA lookup and dead SA removal.
		 */
		if (inp) {
			/* copy old policy into new socket's */
			if (ipsec_copy_pcbpolicy(sotoinpcb(oso)->inp_sp,
			    inp->inp_sp))
				printf("tcp_input: could not copy policy\n");
		}
#ifdef INET6
		else if (in6p) {
			/* copy old policy into new socket's */
			if (ipsec_copy_pcbpolicy(sotoin6pcb(oso)->in6p_sp,
			    in6p->in6p_sp))
				printf("tcp_input: could not copy policy\n");
		}
#endif
	}
#endif

	/*
	 * Give the new socket our cached route reference.
	 */
	if (inp) {
		rtcache_copy(&inp->inp_route, &sc->sc_route);
		rtcache_free(&sc->sc_route);
	}
#ifdef INET6
	else {
		rtcache_copy(&in6p->in6p_route, &sc->sc_route);
		rtcache_free(&sc->sc_route);
	}
#endif

	if (inp) {
		struct sockaddr_in sin;
		memcpy(&sin, src, src->sa_len);
		if (in_pcbconnect(inp, &sin, &lwp0)) {
			goto resetandabort;
		}
	}
#ifdef INET6
	else if (in6p) {
		struct sockaddr_in6 sin6;
		memcpy(&sin6, src, src->sa_len);
		if (src->sa_family == AF_INET) {
			/* IPv4 packet to AF_INET6 socket */
			memset(&sin6, 0, sizeof(sin6));
			sin6.sin6_family = AF_INET6;
			sin6.sin6_len = sizeof(sin6);
			sin6.sin6_port = ((struct sockaddr_in *)src)->sin_port;
			sin6.sin6_addr.s6_addr16[5] = htons(0xffff);
			bcopy(&((struct sockaddr_in *)src)->sin_addr,
				&sin6.sin6_addr.s6_addr32[3],
				sizeof(sin6.sin6_addr.s6_addr32[3]));
		}
		if (in6_pcbconnect(in6p, &sin6, NULL)) {
			goto resetandabort;
		}
	}
#endif
	else {
		goto resetandabort;
	}

	if (inp)
		tp = intotcpcb(inp);
#ifdef INET6
	else if (in6p)
		tp = in6totcpcb(in6p);
#endif
	else
		tp = NULL;
	tp->t_flags = sototcpcb(oso)->t_flags & TF_NODELAY;
	if (sc->sc_request_r_scale != 15) {
		tp->requested_s_scale = sc->sc_requested_s_scale;
		tp->request_r_scale = sc->sc_request_r_scale;
		tp->snd_scale = sc->sc_requested_s_scale;
		tp->rcv_scale = sc->sc_request_r_scale;
		tp->t_flags |= TF_REQ_SCALE|TF_RCVD_SCALE;
	}
	if (sc->sc_flags & SCF_TIMESTAMP)
		tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;
	tp->ts_timebase = sc->sc_timebase;

	tp->t_template = tcp_template(tp);
	if (tp->t_template == 0) {
		tp = tcp_drop(tp, ENOBUFS);	/* destroys socket */
		so = NULL;
		m_freem(m);
		goto abort;
	}

	tp->iss = sc->sc_iss;
	tp->irs = sc->sc_irs;
	tcp_sendseqinit(tp);
	tcp_rcvseqinit(tp);
	tp->t_state = TCPS_SYN_RECEIVED;
	TCP_TIMER_ARM(tp, TCPT_KEEP, tp->t_keepinit);
	TCP_STATINC(TCP_STAT_ACCEPTS);

	if ((sc->sc_flags & SCF_SACK_PERMIT) && tcp_do_sack)
		tp->t_flags |= TF_WILL_SACK;

	if ((sc->sc_flags & SCF_ECN_PERMIT) && tcp_do_ecn)
		tp->t_flags |= TF_ECN_PERMIT;

#ifdef TCP_SIGNATURE
	if (sc->sc_flags & SCF_SIGNATURE)
		tp->t_flags |= TF_SIGNATURE;
#endif

	/* Initialize tp->t_ourmss before we deal with the peer's! */
	tp->t_ourmss = sc->sc_ourmaxseg;
	tcp_mss_from_peer(tp, sc->sc_peermaxseg);

	/*
	 * Initialize the initial congestion window.  If we
	 * had to retransmit the SYN,ACK, we must initialize cwnd
	 * to 1 segment (i.e. the Loss Window).
	 */
	if (sc->sc_rxtshift)
		tp->snd_cwnd = tp->t_peermss;
	else {
		int ss = tcp_init_win;
#ifdef INET
		if (inp != NULL && in_localaddr(inp->inp_faddr))
			ss = tcp_init_win_local;
#endif
#ifdef INET6
		if (in6p != NULL && in6_localaddr(&in6p->in6p_faddr))
			ss = tcp_init_win_local;
#endif
		tp->snd_cwnd = TCP_INITIAL_WINDOW(ss, tp->t_peermss);
	}

	tcp_rmx_rtt(tp);
	tp->snd_wl1 = sc->sc_irs;
	tp->rcv_up = sc->sc_irs + 1;

	/*
	 * This is what whould have happened in tcp_output() when
	 * the SYN,ACK was sent.
	 */
	tp->snd_up = tp->snd_una;
	tp->snd_max = tp->snd_nxt = tp->iss+1;
	TCP_TIMER_ARM(tp, TCPT_REXMT, tp->t_rxtcur);
	if (sc->sc_win > 0 && SEQ_GT(tp->rcv_nxt + sc->sc_win, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + sc->sc_win;
	tp->last_ack_sent = tp->rcv_nxt;
	tp->t_partialacks = -1;
	tp->t_dupacks = 0;

	TCP_STATINC(TCP_STAT_SC_COMPLETED);
	s = splsoftnet();
	syn_cache_put(sc);
	splx(s);
	return (so);

resetandabort:
	(void)tcp_respond(NULL, m, m, th, (tcp_seq)0, th->th_ack, TH_RST);
abort:
	if (so != NULL) {
		(void) soqremque(so, 1);
		(void) soabort(so);
		mutex_enter(softnet_lock);
	}
	s = splsoftnet();
	syn_cache_put(sc);
	splx(s);
	TCP_STATINC(TCP_STAT_SC_ABORTED);
	return ((struct socket *)(-1));
}

/*
 * This function is called when we get a RST for a
 * non-existent connection, so that we can see if the
 * connection is in the syn cache.  If it is, zap it.
 */

void
syn_cache_reset(struct sockaddr *src, struct sockaddr *dst, struct tcphdr *th)
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	int s = splsoftnet();

	if ((sc = syn_cache_lookup(src, dst, &scp)) == NULL) {
		splx(s);
		return;
	}
	if (SEQ_LT(th->th_seq, sc->sc_irs) ||
	    SEQ_GT(th->th_seq, sc->sc_irs+1)) {
		splx(s);
		return;
	}
	syn_cache_rm(sc);
	TCP_STATINC(TCP_STAT_SC_RESET);
	syn_cache_put(sc);	/* calls pool_put but see spl above */
	splx(s);
}

void
syn_cache_unreach(const struct sockaddr *src, const struct sockaddr *dst,
    struct tcphdr *th)
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	int s;

	s = splsoftnet();
	if ((sc = syn_cache_lookup(src, dst, &scp)) == NULL) {
		splx(s);
		return;
	}
	/* If the sequence number != sc_iss, then it's a bogus ICMP msg */
	if (ntohl (th->th_seq) != sc->sc_iss) {
		splx(s);
		return;
	}

	/*
	 * If we've retransmitted 3 times and this is our second error,
	 * we remove the entry.  Otherwise, we allow it to continue on.
	 * This prevents us from incorrectly nuking an entry during a
	 * spurious network outage.
	 *
	 * See tcp_notify().
	 */
	if ((sc->sc_flags & SCF_UNREACH) == 0 || sc->sc_rxtshift < 3) {
		sc->sc_flags |= SCF_UNREACH;
		splx(s);
		return;
	}

	syn_cache_rm(sc);
	TCP_STATINC(TCP_STAT_SC_UNREACH);
	syn_cache_put(sc);	/* calls pool_put but see spl above */
	splx(s);
}

/*
 * Given a LISTEN socket and an inbound SYN request, add
 * this to the syn cache, and send back a segment:
 *	<SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
 * to the source.
 *
 * IMPORTANT NOTE: We do _NOT_ ACK data that might accompany the SYN.
 * Doing so would require that we hold onto the data and deliver it
 * to the application.  However, if we are the target of a SYN-flood
 * DoS attack, an attacker could send data which would eventually
 * consume all available buffer space if it were ACKed.  By not ACKing
 * the data, we avoid this DoS scenario.
 */

int
syn_cache_add(struct sockaddr *src, struct sockaddr *dst, struct tcphdr *th,
    unsigned int hlen, struct socket *so, struct mbuf *m, u_char *optp,
    int optlen, struct tcp_opt_info *oi)
{
	struct tcpcb tb, *tp;
	long win;
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	struct mbuf *ipopts;
	struct tcp_opt_info opti;
	int s;

	tp = sototcpcb(so);

	memset(&opti, 0, sizeof(opti));

	/*
	 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
	 *
	 * Note this check is performed in tcp_input() very early on.
	 */

	/*
	 * Initialize some local state.
	 */
	win = sbspace(&so->so_rcv);
	if (win > TCP_MAXWIN)
		win = TCP_MAXWIN;

	switch (src->sa_family) {
#ifdef INET
	case AF_INET:
		/*
		 * Remember the IP options, if any.
		 */
		ipopts = ip_srcroute();
		break;
#endif
	default:
		ipopts = NULL;
	}

#ifdef TCP_SIGNATURE
	if (optp || (tp->t_flags & TF_SIGNATURE))
#else
	if (optp)
#endif
	{
		tb.t_flags = tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;
#ifdef TCP_SIGNATURE
		tb.t_flags |= (tp->t_flags & TF_SIGNATURE);
#endif
		tb.t_state = TCPS_LISTEN;
		if (tcp_dooptions(&tb, optp, optlen, th, m, m->m_pkthdr.len -
		    sizeof(struct tcphdr) - optlen - hlen, oi) < 0)
			return (0);
	} else
		tb.t_flags = 0;

	/*
	 * See if we already have an entry for this connection.
	 * If we do, resend the SYN,ACK.  We do not count this
	 * as a retransmission (XXX though maybe we should).
	 */
	if ((sc = syn_cache_lookup(src, dst, &scp)) != NULL) {
		TCP_STATINC(TCP_STAT_SC_DUPESYN);
		if (ipopts) {
			/*
			 * If we were remembering a previous source route,
			 * forget it and use the new one we've been given.
			 */
			if (sc->sc_ipopts)
				(void) m_free(sc->sc_ipopts);
			sc->sc_ipopts = ipopts;
		}
		sc->sc_timestamp = tb.ts_recent;
		if (syn_cache_respond(sc, m) == 0) {
			uint64_t *tcps = TCP_STAT_GETREF();
			tcps[TCP_STAT_SNDACKS]++;
			tcps[TCP_STAT_SNDTOTAL]++;
			TCP_STAT_PUTREF();
		}
		return (1);
	}

	s = splsoftnet();
	sc = pool_get(&syn_cache_pool, PR_NOWAIT);
	splx(s);
	if (sc == NULL) {
		if (ipopts)
			(void) m_free(ipopts);
		return (0);
	}

	/*
	 * Fill in the cache, and put the necessary IP and TCP
	 * options into the reply.
	 */
	memset(sc, 0, sizeof(struct syn_cache));
	callout_init(&sc->sc_timer, CALLOUT_MPSAFE);
	bcopy(src, &sc->sc_src, src->sa_len);
	bcopy(dst, &sc->sc_dst, dst->sa_len);
	sc->sc_flags = 0;
	sc->sc_ipopts = ipopts;
	sc->sc_irs = th->th_seq;
	switch (src->sa_family) {
#ifdef INET
	case AF_INET:
	    {
		struct sockaddr_in *srcin = (void *) src;
		struct sockaddr_in *dstin = (void *) dst;

		sc->sc_iss = tcp_new_iss1(&dstin->sin_addr,
		    &srcin->sin_addr, dstin->sin_port,
		    srcin->sin_port, sizeof(dstin->sin_addr), 0);
		break;
	    }
#endif /* INET */
#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *srcin6 = (void *) src;
		struct sockaddr_in6 *dstin6 = (void *) dst;

		sc->sc_iss = tcp_new_iss1(&dstin6->sin6_addr,
		    &srcin6->sin6_addr, dstin6->sin6_port,
		    srcin6->sin6_port, sizeof(dstin6->sin6_addr), 0);
		break;
	    }
#endif /* INET6 */
	}
	sc->sc_peermaxseg = oi->maxseg;
	sc->sc_ourmaxseg = tcp_mss_to_advertise(m->m_flags & M_PKTHDR ?
						m->m_pkthdr.rcvif : NULL,
						sc->sc_src.sa.sa_family);
	sc->sc_win = win;
	sc->sc_timebase = tcp_now - 1;	/* see tcp_newtcpcb() */
	sc->sc_timestamp = tb.ts_recent;
	if ((tb.t_flags & (TF_REQ_TSTMP|TF_RCVD_TSTMP)) ==
	    (TF_REQ_TSTMP|TF_RCVD_TSTMP))
		sc->sc_flags |= SCF_TIMESTAMP;
	if ((tb.t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
	    (TF_RCVD_SCALE|TF_REQ_SCALE)) {
		sc->sc_requested_s_scale = tb.requested_s_scale;
		sc->sc_request_r_scale = 0;
		/*
		 * Pick the smallest possible scaling factor that
		 * will still allow us to scale up to sb_max.
		 *
		 * We do this because there are broken firewalls that
		 * will corrupt the window scale option, leading to
		 * the other endpoint believing that our advertised
		 * window is unscaled.  At scale factors larger than
		 * 5 the unscaled window will drop below 1500 bytes,
		 * leading to serious problems when traversing these
		 * broken firewalls.
		 *
		 * With the default sbmax of 256K, a scale factor
		 * of 3 will be chosen by this algorithm.  Those who
		 * choose a larger sbmax should watch out
		 * for the compatiblity problems mentioned above.
		 *
		 * RFC1323: The Window field in a SYN (i.e., a <SYN>
		 * or <SYN,ACK>) segment itself is never scaled.
		 */
		while (sc->sc_request_r_scale < TCP_MAX_WINSHIFT &&
		    (TCP_MAXWIN << sc->sc_request_r_scale) < sb_max)
			sc->sc_request_r_scale++;
	} else {
		sc->sc_requested_s_scale = 15;
		sc->sc_request_r_scale = 15;
	}
	if ((tb.t_flags & TF_SACK_PERMIT) && tcp_do_sack)
		sc->sc_flags |= SCF_SACK_PERMIT;

	/*
	 * ECN setup packet recieved.
	 */
	if ((th->th_flags & (TH_ECE|TH_CWR)) && tcp_do_ecn)
		sc->sc_flags |= SCF_ECN_PERMIT;

#ifdef TCP_SIGNATURE
	if (tb.t_flags & TF_SIGNATURE)
		sc->sc_flags |= SCF_SIGNATURE;
#endif
	sc->sc_tp = tp;
	if (syn_cache_respond(sc, m) == 0) {
		uint64_t *tcps = TCP_STAT_GETREF();
		tcps[TCP_STAT_SNDACKS]++;
		tcps[TCP_STAT_SNDTOTAL]++;
		TCP_STAT_PUTREF();
		syn_cache_insert(sc, tp);
	} else {
		s = splsoftnet();
		/*
		 * syn_cache_put() will try to schedule the timer, so
		 * we need to initialize it
		 */
		SYN_CACHE_TIMER_ARM(sc);
		syn_cache_put(sc);
		splx(s);
		TCP_STATINC(TCP_STAT_SC_DROPPED);
	}
	return (1);
}

/*
 * syn_cache_respond: (re)send SYN+ACK.
 *
 * returns 0 on success.  otherwise returns an errno, typically ENOBUFS.
 */

int
syn_cache_respond(struct syn_cache *sc, struct mbuf *m)
{
#ifdef INET6
	struct rtentry *rt;
#endif
	struct route *ro;
	u_int8_t *optp;
	int optlen, error;
	u_int16_t tlen;
	struct ip *ip = NULL;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
	struct tcpcb *tp = NULL;
	struct tcphdr *th;
	u_int hlen;
	struct socket *so;

	ro = &sc->sc_route;
	switch (sc->sc_src.sa.sa_family) {
	case AF_INET:
		hlen = sizeof(struct ip);
		break;
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif
	default:
		if (m)
			m_freem(m);
		return (EAFNOSUPPORT);
	}

	/* Compute the size of the TCP options. */
	optlen = 4 + (sc->sc_request_r_scale != 15 ? 4 : 0) +
	    ((sc->sc_flags & SCF_SACK_PERMIT) ? (TCPOLEN_SACK_PERMITTED + 2) : 0) +
#ifdef TCP_SIGNATURE
	    ((sc->sc_flags & SCF_SIGNATURE) ? (TCPOLEN_SIGNATURE + 2) : 0) +
#endif
	    ((sc->sc_flags & SCF_TIMESTAMP) ? TCPOLEN_TSTAMP_APPA : 0);

	tlen = hlen + sizeof(struct tcphdr) + optlen;

	/*
	 * Create the IP+TCP header from scratch.
	 */
	if (m)
		m_freem(m);
#ifdef DIAGNOSTIC
	if (max_linkhdr + tlen > MCLBYTES)
		return (ENOBUFS);
#endif
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m && (max_linkhdr + tlen) > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			m = NULL;
		}
	}
	if (m == NULL)
		return (ENOBUFS);
	MCLAIM(m, &tcp_tx_mowner);

	/* Fixup the mbuf. */
	m->m_data += max_linkhdr;
	m->m_len = m->m_pkthdr.len = tlen;
	if (sc->sc_tp) {
		tp = sc->sc_tp;
		if (tp->t_inpcb)
			so = tp->t_inpcb->inp_socket;
#ifdef INET6
		else if (tp->t_in6pcb)
			so = tp->t_in6pcb->in6p_socket;
#endif
		else
			so = NULL;
	} else
		so = NULL;
	m->m_pkthdr.rcvif = NULL;
	memset(mtod(m, u_char *), 0, tlen);

	switch (sc->sc_src.sa.sa_family) {
	case AF_INET:
		ip = mtod(m, struct ip *);
		ip->ip_v = 4;
		ip->ip_dst = sc->sc_src.sin.sin_addr;
		ip->ip_src = sc->sc_dst.sin.sin_addr;
		ip->ip_p = IPPROTO_TCP;
		th = (struct tcphdr *)(ip + 1);
		th->th_dport = sc->sc_src.sin.sin_port;
		th->th_sport = sc->sc_dst.sin.sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_dst = sc->sc_src.sin6.sin6_addr;
		ip6->ip6_src = sc->sc_dst.sin6.sin6_addr;
		ip6->ip6_nxt = IPPROTO_TCP;
		/* ip6_plen will be updated in ip6_output() */
		th = (struct tcphdr *)(ip6 + 1);
		th->th_dport = sc->sc_src.sin6.sin6_port;
		th->th_sport = sc->sc_dst.sin6.sin6_port;
		break;
#endif
	default:
		th = NULL;
	}

	th->th_seq = htonl(sc->sc_iss);
	th->th_ack = htonl(sc->sc_irs + 1);
	th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
	th->th_flags = TH_SYN|TH_ACK;
	th->th_win = htons(sc->sc_win);
	/* th_sum already 0 */
	/* th_urp already 0 */

	/* Tack on the TCP options. */
	optp = (u_int8_t *)(th + 1);
	*optp++ = TCPOPT_MAXSEG;
	*optp++ = 4;
	*optp++ = (sc->sc_ourmaxseg >> 8) & 0xff;
	*optp++ = sc->sc_ourmaxseg & 0xff;

	if (sc->sc_request_r_scale != 15) {
		*((u_int32_t *)optp) = htonl(TCPOPT_NOP << 24 |
		    TCPOPT_WINDOW << 16 | TCPOLEN_WINDOW << 8 |
		    sc->sc_request_r_scale);
		optp += 4;
	}

	if (sc->sc_flags & SCF_TIMESTAMP) {
		u_int32_t *lp = (u_int32_t *)(optp);
		/* Form timestamp option as shown in appendix A of RFC 1323. */
		*lp++ = htonl(TCPOPT_TSTAMP_HDR);
		*lp++ = htonl(SYN_CACHE_TIMESTAMP(sc));
		*lp   = htonl(sc->sc_timestamp);
		optp += TCPOLEN_TSTAMP_APPA;
	}

	if (sc->sc_flags & SCF_SACK_PERMIT) {
		u_int8_t *p = optp;

		/* Let the peer know that we will SACK. */
		p[0] = TCPOPT_SACK_PERMITTED;
		p[1] = 2;
		p[2] = TCPOPT_NOP;
		p[3] = TCPOPT_NOP;
		optp += 4;
	}

	/*
	 * Send ECN SYN-ACK setup packet.
	 * Routes can be asymetric, so, even if we receive a packet
	 * with ECE and CWR set, we must not assume no one will block
	 * the ECE packet we are about to send.
	 */
	if ((sc->sc_flags & SCF_ECN_PERMIT) && tp &&
	    SEQ_GEQ(tp->snd_nxt, tp->snd_max)) {
		th->th_flags |= TH_ECE;
		TCP_STATINC(TCP_STAT_ECN_SHS);

		/*
		 * draft-ietf-tcpm-ecnsyn-00.txt
		 *
		 * "[...] a TCP node MAY respond to an ECN-setup
		 * SYN packet by setting ECT in the responding
		 * ECN-setup SYN/ACK packet, indicating to routers 
		 * that the SYN/ACK packet is ECN-Capable.
		 * This allows a congested router along the path
		 * to mark the packet instead of dropping the
		 * packet as an indication of congestion."
		 *
		 * "[...] There can be a great benefit in setting
		 * an ECN-capable codepoint in SYN/ACK packets [...]
		 * Congestion is  most likely to occur in
		 * the server-to-client direction.  As a result,
		 * setting an ECN-capable codepoint in SYN/ACK
		 * packets can reduce the occurence of three-second
		 * retransmit timeouts resulting from the drop
		 * of SYN/ACK packets."
		 *
		 * Page 4 and 6, January 2006.
		 */

		switch (sc->sc_src.sa.sa_family) {
#ifdef INET
		case AF_INET:
			ip->ip_tos |= IPTOS_ECN_ECT0;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			ip6->ip6_flow |= htonl(IPTOS_ECN_ECT0 << 20);
			break;
#endif
		}
		TCP_STATINC(TCP_STAT_ECN_ECT);
	}

#ifdef TCP_SIGNATURE
	if (sc->sc_flags & SCF_SIGNATURE) {
		struct secasvar *sav;
		u_int8_t *sigp;

		sav = tcp_signature_getsav(m, th);

		if (sav == NULL) {
			if (m)
				m_freem(m);
			return (EPERM);
		}

		*optp++ = TCPOPT_SIGNATURE;
		*optp++ = TCPOLEN_SIGNATURE;
		sigp = optp;
		memset(optp, 0, TCP_SIGLEN);
		optp += TCP_SIGLEN;
		*optp++ = TCPOPT_NOP;
		*optp++ = TCPOPT_EOL;

		(void)tcp_signature(m, th, hlen, sav, sigp);

		key_sa_recordxfer(sav, m);
		KEY_FREESAV(&sav);
	}
#endif

	/* Compute the packet's checksum. */
	switch (sc->sc_src.sa.sa_family) {
	case AF_INET:
		ip->ip_len = htons(tlen - hlen);
		th->th_sum = 0;
		th->th_sum = in4_cksum(m, IPPROTO_TCP, hlen, tlen - hlen);
		break;
#ifdef INET6
	case AF_INET6:
		ip6->ip6_plen = htons(tlen - hlen);
		th->th_sum = 0;
		th->th_sum = in6_cksum(m, IPPROTO_TCP, hlen, tlen - hlen);
		break;
#endif
	}

	/*
	 * Fill in some straggling IP bits.  Note the stack expects
	 * ip_len to be in host order, for convenience.
	 */
	switch (sc->sc_src.sa.sa_family) {
#ifdef INET
	case AF_INET:
		ip->ip_len = htons(tlen);
		ip->ip_ttl = ip_defttl;
		/* XXX tos? */
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_plen = htons(tlen - hlen);
		/* ip6_hlim will be initialized afterwards */
		/* XXX flowlabel? */
		break;
#endif
	}

	/* XXX use IPsec policy on listening socket, on SYN ACK */
	tp = sc->sc_tp;

	switch (sc->sc_src.sa.sa_family) {
#ifdef INET
	case AF_INET:
		error = ip_output(m, sc->sc_ipopts, ro,
		    (ip_mtudisc ? IP_MTUDISC : 0),
		    NULL, so);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ip6->ip6_hlim = in6_selecthlim(NULL,
		    (rt = rtcache_validate(ro)) != NULL ? rt->rt_ifp : NULL);

		error = ip6_output(m, NULL /*XXX*/, ro, 0, NULL, so, NULL);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		break;
	}
	return (error);
}
