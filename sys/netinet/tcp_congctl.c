/*	$NetBSD: tcp_congctl.c,v 1.20 2015/08/24 22:21:26 pooka Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2001, 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcp_congctl.c,v 1.20 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_tcp_debug.h"
#include "opt_tcp_congctl.h"
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
#include <sys/mutex.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

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
#endif

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_congctl.h>
#ifdef TCP_DEBUG
#include <netinet/tcp_debug.h>
#endif

/*
 * TODO:
 *   consider separating the actual implementations in another file.
 */

static void tcp_common_congestion_exp(struct tcpcb *, int, int);

static int  tcp_reno_do_fast_retransmit(struct tcpcb *, const struct tcphdr *);
static int  tcp_reno_fast_retransmit(struct tcpcb *, const struct tcphdr *);
static void tcp_reno_slow_retransmit(struct tcpcb *);
static void tcp_reno_fast_retransmit_newack(struct tcpcb *,
    const struct tcphdr *);
static void tcp_reno_newack(struct tcpcb *, const struct tcphdr *);
static void tcp_reno_congestion_exp(struct tcpcb *tp);

static int  tcp_newreno_fast_retransmit(struct tcpcb *, const struct tcphdr *);
static void tcp_newreno_fast_retransmit_newack(struct tcpcb *,
	const struct tcphdr *);
static void tcp_newreno_newack(struct tcpcb *, const struct tcphdr *);

static int tcp_cubic_fast_retransmit(struct tcpcb *, const struct tcphdr *);
static void tcp_cubic_slow_retransmit(struct tcpcb *tp);
static void tcp_cubic_newack(struct tcpcb *, const struct tcphdr *);
static void tcp_cubic_congestion_exp(struct tcpcb *);

static void tcp_congctl_fillnames(void);

extern int tcprexmtthresh;

MALLOC_DEFINE(M_TCPCONGCTL, "tcpcongctl", "TCP congestion control structures");

/* currently selected global congestion control */
char tcp_congctl_global_name[TCPCC_MAXLEN];

/* available global congestion control algorithms */
char tcp_congctl_avail[10 * TCPCC_MAXLEN];

/*
 * Used to list the available congestion control algorithms.
 */
TAILQ_HEAD(, tcp_congctlent) tcp_congctlhd =
    TAILQ_HEAD_INITIALIZER(tcp_congctlhd);

static struct tcp_congctlent * tcp_congctl_global;

static kmutex_t tcp_congctl_mtx;

void
tcp_congctl_init(void)
{
	int r __diagused;
	
	mutex_init(&tcp_congctl_mtx, MUTEX_DEFAULT, IPL_NONE);

	/* Base algorithms. */
	r = tcp_congctl_register("reno", &tcp_reno_ctl);
	KASSERT(r == 0);
	r = tcp_congctl_register("newreno", &tcp_newreno_ctl);
	KASSERT(r == 0);
	r = tcp_congctl_register("cubic", &tcp_cubic_ctl);
	KASSERT(r == 0);

	/* NewReno is the default. */
#ifndef TCP_CONGCTL_DEFAULT
#define TCP_CONGCTL_DEFAULT "newreno"
#endif

	r = tcp_congctl_select(NULL, TCP_CONGCTL_DEFAULT);
	KASSERT(r == 0);
}

/*
 * Register a congestion algorithm and select it if we have none.
 */
int
tcp_congctl_register(const char *name, const struct tcp_congctl *tcc)
{
	struct tcp_congctlent *ntcc, *tccp;

	TAILQ_FOREACH(tccp, &tcp_congctlhd, congctl_ent) 
		if (!strcmp(name, tccp->congctl_name)) {
			/* name already registered */
			return EEXIST;
		}

	ntcc = malloc(sizeof(*ntcc), M_TCPCONGCTL, M_WAITOK|M_ZERO);

	strlcpy(ntcc->congctl_name, name, sizeof(ntcc->congctl_name) - 1);
	ntcc->congctl_ctl = tcc;

	TAILQ_INSERT_TAIL(&tcp_congctlhd, ntcc, congctl_ent);
	tcp_congctl_fillnames();

	if (TAILQ_FIRST(&tcp_congctlhd) == ntcc)
		tcp_congctl_select(NULL, name);
		
	return 0;
}

int
tcp_congctl_unregister(const char *name)
{
	struct tcp_congctlent *tccp, *rtccp;
	unsigned int size;
	
	rtccp = NULL;
	size = 0;
	TAILQ_FOREACH(tccp, &tcp_congctlhd, congctl_ent) {
		if (!strcmp(name, tccp->congctl_name))
			rtccp = tccp;
		size++;
	}
	
	if (!rtccp)
		return ENOENT;

	if (size <= 1 || tcp_congctl_global == rtccp || rtccp->congctl_refcnt)
		return EBUSY;

	TAILQ_REMOVE(&tcp_congctlhd, rtccp, congctl_ent);
	free(rtccp, M_TCPCONGCTL);
	tcp_congctl_fillnames();

	return 0;
}

/*
 * Select a congestion algorithm by name.
 */
int
tcp_congctl_select(struct tcpcb *tp, const char *name)
{
	struct tcp_congctlent *tccp, *old_tccp, *new_tccp;
	bool old_found, new_found;

	KASSERT(name);

	old_found = (tp == NULL || tp->t_congctl == NULL);
	old_tccp = NULL;
	new_found = false;
	new_tccp = NULL;

	TAILQ_FOREACH(tccp, &tcp_congctlhd, congctl_ent) {
		if (!old_found && tccp->congctl_ctl == tp->t_congctl) {
			old_tccp = tccp;
			old_found = true;
		}

		if (!new_found && !strcmp(name, tccp->congctl_name)) {
			new_tccp = tccp;
			new_found = true;
		}

		if (new_found && old_found) {
			if (tp) {
				mutex_enter(&tcp_congctl_mtx);
				if (old_tccp)
					old_tccp->congctl_refcnt--;
				tp->t_congctl = new_tccp->congctl_ctl;
				new_tccp->congctl_refcnt++;
				mutex_exit(&tcp_congctl_mtx);
			} else {
				tcp_congctl_global = new_tccp;
				strlcpy(tcp_congctl_global_name,
				    new_tccp->congctl_name,
				    sizeof(tcp_congctl_global_name) - 1);
			}
			return 0;
		}
	}

	return EINVAL;
}

void
tcp_congctl_release(struct tcpcb *tp)
{
	struct tcp_congctlent *tccp;

	KASSERT(tp->t_congctl);
	
	TAILQ_FOREACH(tccp, &tcp_congctlhd, congctl_ent) {
		if (tccp->congctl_ctl == tp->t_congctl) {
			tccp->congctl_refcnt--;
			return;
		}
	}
}

/*
 * Returns the name of a congestion algorithm.
 */
const char *
tcp_congctl_bystruct(const struct tcp_congctl *tcc)
{
	struct tcp_congctlent *tccp;
	
	KASSERT(tcc);
	
	TAILQ_FOREACH(tccp, &tcp_congctlhd, congctl_ent)
		if (tccp->congctl_ctl == tcc)
			return tccp->congctl_name;

	return NULL;
}

static void
tcp_congctl_fillnames(void)
{
	struct tcp_congctlent *tccp;
	const char *delim = " ";
	
	tcp_congctl_avail[0] = '\0';
	TAILQ_FOREACH(tccp, &tcp_congctlhd, congctl_ent) {
		strlcat(tcp_congctl_avail, tccp->congctl_name,
		    sizeof(tcp_congctl_avail) - 1);
		if (TAILQ_NEXT(tccp, congctl_ent))
			strlcat(tcp_congctl_avail, delim, 
			    sizeof(tcp_congctl_avail) - 1);
	}	
	
}

/* ------------------------------------------------------------------------ */

/*
 * Common stuff
 */

/* Window reduction (1-beta) for [New]Reno: 0.5 */
#define RENO_BETAA 1
#define RENO_BETAB 2
/* Window reduction (1-beta) for Cubic: 0.8 */
#define CUBIC_BETAA 4
#define CUBIC_BETAB 5
/* Draft Rhee Section 4.1 */
#define CUBIC_CA 4
#define CUBIC_CB 10

static void
tcp_common_congestion_exp(struct tcpcb *tp, int betaa, int betab)
{
	u_int win;

	/* 
	 * Reduce the congestion window and the slow start threshold.
	 */
	win = min(tp->snd_wnd, tp->snd_cwnd) * betaa / betab / tp->t_segsz;
	if (win < 2)
		win = 2;

	tp->snd_ssthresh = win * tp->t_segsz;
	tp->snd_recover = tp->snd_max;
	tp->snd_cwnd = tp->snd_ssthresh;

	/*
	 * When using TCP ECN, notify the peer that
	 * we reduced the cwnd.
	 */
	if (TCP_ECN_ALLOWED(tp))
		tp->t_flags |= TF_ECN_SND_CWR;
}


/* ------------------------------------------------------------------------ */

/*
 * TCP/Reno congestion control.
 */
static void
tcp_reno_congestion_exp(struct tcpcb *tp)
{

	tcp_common_congestion_exp(tp, RENO_BETAA, RENO_BETAB);
}

static int
tcp_reno_do_fast_retransmit(struct tcpcb *tp, const struct tcphdr *th)
{
	/*
	 * Dup acks mean that packets have left the
	 * network (they're now cached at the receiver)
	 * so bump cwnd by the amount in the receiver
	 * to keep a constant cwnd packets in the
	 * network.
	 *
	 * If we are using TCP/SACK, then enter
	 * Fast Recovery if the receiver SACKs
	 * data that is tcprexmtthresh * MSS
	 * bytes past the last ACKed segment,
	 * irrespective of the number of DupAcks.
	 */
	
	tcp_seq onxt = tp->snd_nxt;

	tp->t_partialacks = 0;
	TCP_TIMER_DISARM(tp, TCPT_REXMT);
	tp->t_rtttime = 0;
	if (TCP_SACK_ENABLED(tp)) {
		tp->t_dupacks = tcprexmtthresh;
		tp->sack_newdata = tp->snd_nxt;
		tp->snd_cwnd = tp->t_segsz;
		(void) tcp_output(tp);
		return 0;
	}
	tp->snd_nxt = th->th_ack;
	tp->snd_cwnd = tp->t_segsz;
	(void) tcp_output(tp);
	tp->snd_cwnd = tp->snd_ssthresh + tp->t_segsz * tp->t_dupacks;
	if (SEQ_GT(onxt, tp->snd_nxt))
		tp->snd_nxt = onxt;

	return 0;
}

static int
tcp_reno_fast_retransmit(struct tcpcb *tp, const struct tcphdr *th)
{

	/*
	 * We know we're losing at the current
	 * window size so do congestion avoidance
	 * (set ssthresh to half the current window
	 * and pull our congestion window back to
	 * the new ssthresh).
	 */

	tcp_reno_congestion_exp(tp);
	return tcp_reno_do_fast_retransmit(tp, th);
}

static void
tcp_reno_slow_retransmit(struct tcpcb *tp)
{
	u_int win;

	/*
	 * Close the congestion window down to one segment
	 * (we'll open it by one segment for each ack we get).
	 * Since we probably have a window's worth of unacked
	 * data accumulated, this "slow start" keeps us from
	 * dumping all that data as back-to-back packets (which
	 * might overwhelm an intermediate gateway).
	 *
	 * There are two phases to the opening: Initially we
	 * open by one mss on each ack.  This makes the window
	 * size increase exponentially with time.  If the
	 * window is larger than the path can handle, this
	 * exponential growth results in dropped packet(s)
	 * almost immediately.  To get more time between
	 * drops but still "push" the network to take advantage
	 * of improving conditions, we switch from exponential
	 * to linear window opening at some threshhold size.
	 * For a threshhold, we use half the current window
	 * size, truncated to a multiple of the mss.
	 *
	 * (the minimum cwnd that will give us exponential
	 * growth is 2 mss.  We don't allow the threshhold
	 * to go below this.)
	 */

	win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_segsz;
	if (win < 2)
		win = 2;
	/* Loss Window MUST be one segment. */
	tp->snd_cwnd = tp->t_segsz;
	tp->snd_ssthresh = win * tp->t_segsz;
	tp->t_partialacks = -1;
	tp->t_dupacks = 0;
	tp->t_bytes_acked = 0;

	if (TCP_ECN_ALLOWED(tp))
		tp->t_flags |= TF_ECN_SND_CWR;
}

static void
tcp_reno_fast_retransmit_newack(struct tcpcb *tp,
    const struct tcphdr *th)
{
	if (tp->t_partialacks < 0) {
		/*
		 * We were not in fast recovery.  Reset the duplicate ack
		 * counter.
		 */
		tp->t_dupacks = 0;
	} else {
		/*
		 * Clamp the congestion window to the crossover point and
		 * exit fast recovery.
		 */
		if (tp->snd_cwnd > tp->snd_ssthresh)
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_partialacks = -1;
		tp->t_dupacks = 0;
		tp->t_bytes_acked = 0;
		if (TCP_SACK_ENABLED(tp) && SEQ_GT(th->th_ack, tp->snd_fack))
			tp->snd_fack = th->th_ack;
	}
}

static void
tcp_reno_newack(struct tcpcb *tp, const struct tcphdr *th)
{
	/*
	 * When new data is acked, open the congestion window.
	 */

	u_int cw = tp->snd_cwnd;
	u_int incr = tp->t_segsz;

	if (tcp_do_abc) {

		/*
		 * RFC 3465 Appropriate Byte Counting (ABC)
		 */

		int acked = th->th_ack - tp->snd_una;

		if (cw >= tp->snd_ssthresh) {
			tp->t_bytes_acked += acked;
			if (tp->t_bytes_acked >= cw) {
				/* Time to increase the window. */
				tp->t_bytes_acked -= cw;
			} else {
				/* No need to increase yet. */
				incr = 0;
			}
		} else {
			/*
			 * use 2*SMSS or 1*SMSS for the "L" param,
			 * depending on sysctl setting.
			 *
			 * (See RFC 3465 2.3 Choosing the Limit)
			 */
			u_int abc_lim;

			abc_lim = (tcp_abc_aggressive == 0 ||
			    tp->snd_nxt != tp->snd_max) ? incr : incr * 2;
			incr = min(acked, abc_lim);
		}
	} else {

		/*
		 * If the window gives us less than ssthresh packets
		 * in flight, open exponentially (segsz per packet).
		 * Otherwise open linearly: segsz per window
		 * (segsz^2 / cwnd per packet).
		 */

		if (cw >= tp->snd_ssthresh) {
			incr = incr * incr / cw;
		}
	}

	tp->snd_cwnd = min(cw + incr, TCP_MAXWIN << tp->snd_scale);
}

const struct tcp_congctl tcp_reno_ctl = {
	.fast_retransmit = tcp_reno_fast_retransmit,
	.slow_retransmit = tcp_reno_slow_retransmit,
	.fast_retransmit_newack = tcp_reno_fast_retransmit_newack,
	.newack = tcp_reno_newack,
	.cong_exp = tcp_reno_congestion_exp,
};

/*
 * TCP/NewReno Congestion control.
 */
static int
tcp_newreno_fast_retransmit(struct tcpcb *tp, const struct tcphdr *th)
{

	if (SEQ_LT(th->th_ack, tp->snd_high)) {
		/*
		 * False fast retransmit after timeout.
		 * Do not enter fast recovery
		 */
		tp->t_dupacks = 0;
		return 1;
	}
	/*
	 * Fast retransmit is same as reno.
	 */
	return tcp_reno_fast_retransmit(tp, th);
}

/*
 * Implement the NewReno response to a new ack, checking for partial acks in
 * fast recovery.
 */
static void
tcp_newreno_fast_retransmit_newack(struct tcpcb *tp, const struct tcphdr *th)
{
	if (tp->t_partialacks < 0) {
		/*
		 * We were not in fast recovery.  Reset the duplicate ack
		 * counter.
		 */
		tp->t_dupacks = 0;
	} else if (SEQ_LT(th->th_ack, tp->snd_recover)) {
		/*
		 * This is a partial ack.  Retransmit the first unacknowledged
		 * segment and deflate the congestion window by the amount of
		 * acknowledged data.  Do not exit fast recovery.
		 */
		tcp_seq onxt = tp->snd_nxt;
		u_long ocwnd = tp->snd_cwnd;
		int sack_num_segs = 1, sack_bytes_rxmt = 0;

		/*
		 * snd_una has not yet been updated and the socket's send
		 * buffer has not yet drained off the ACK'd data, so we
		 * have to leave snd_una as it was to get the correct data
		 * offset in tcp_output().
		 */
		tp->t_partialacks++;
		TCP_TIMER_DISARM(tp, TCPT_REXMT);
		tp->t_rtttime = 0;
		tp->snd_nxt = th->th_ack;

		if (TCP_SACK_ENABLED(tp)) {
			/*
			 * Partial ack handling within a sack recovery episode.
			 * Keeping this very simple for now. When a partial ack
			 * is received, force snd_cwnd to a value that will
			 * allow the sender to transmit no more than 2 segments.
			 * If necessary, a fancier scheme can be adopted at a
			 * later point, but for now, the goal is to prevent the
			 * sender from bursting a large amount of data in the
			 * midst of sack recovery.
		 	 */

			/*
			 * send one or 2 segments based on how much
			 * new data was acked
			 */
			if (((th->th_ack - tp->snd_una) / tp->t_segsz) > 2)
				sack_num_segs = 2;
			(void)tcp_sack_output(tp, &sack_bytes_rxmt);
			tp->snd_cwnd = sack_bytes_rxmt +
			    (tp->snd_nxt - tp->sack_newdata) +
			    sack_num_segs * tp->t_segsz;
			tp->t_flags |= TF_ACKNOW;
			(void) tcp_output(tp);
		} else {
			/*
			 * Set snd_cwnd to one segment beyond ACK'd offset
			 * snd_una is not yet updated when we're called
			 */
			tp->snd_cwnd = tp->t_segsz + (th->th_ack - tp->snd_una);
			(void) tcp_output(tp);
			tp->snd_cwnd = ocwnd;
			if (SEQ_GT(onxt, tp->snd_nxt))
				tp->snd_nxt = onxt;
			/*
			 * Partial window deflation.  Relies on fact that
			 * tp->snd_una not updated yet.
		 	 */
			tp->snd_cwnd -= (th->th_ack - tp->snd_una -
			    tp->t_segsz);
		}
	} else {
		/*
		 * Complete ack.  Inflate the congestion window to ssthresh
		 * and exit fast recovery.
		 *
		 * Window inflation should have left us with approx.
		 * snd_ssthresh outstanding data.  But in case we
		 * would be inclined to send a burst, better to do
		 * it via the slow start mechanism.
		 */
		if (SEQ_SUB(tp->snd_max, th->th_ack) < tp->snd_ssthresh)
			tp->snd_cwnd = SEQ_SUB(tp->snd_max, th->th_ack)
			    + tp->t_segsz;
		else
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_partialacks = -1;
		tp->t_dupacks = 0;
		tp->t_bytes_acked = 0;
		if (TCP_SACK_ENABLED(tp) && SEQ_GT(th->th_ack, tp->snd_fack))
			tp->snd_fack = th->th_ack;
	}
}

static void
tcp_newreno_newack(struct tcpcb *tp, const struct tcphdr *th)
{
	/*
	 * If we are still in fast recovery (meaning we are using
	 * NewReno and we have only received partial acks), do not
	 * inflate the window yet.
	 */
	if (tp->t_partialacks < 0)
		tcp_reno_newack(tp, th);
}


const struct tcp_congctl tcp_newreno_ctl = {
	.fast_retransmit = tcp_newreno_fast_retransmit,
	.slow_retransmit = tcp_reno_slow_retransmit,
	.fast_retransmit_newack = tcp_newreno_fast_retransmit_newack,
	.newack = tcp_newreno_newack,
	.cong_exp = tcp_reno_congestion_exp,
};

/*
 * CUBIC - http://tools.ietf.org/html/draft-rhee-tcpm-cubic-02
 */

/* Cubic prototypes */
static void	tcp_cubic_update_ctime(struct tcpcb *tp);
static uint32_t	tcp_cubic_diff_ctime(struct tcpcb *);
static uint32_t	tcp_cubic_cbrt(uint32_t);
static ulong	tcp_cubic_getW(struct tcpcb *, uint32_t, uint32_t);

/* Cubic TIME functions - XXX I don't like using timevals and microuptime */
/*
 * Set congestion timer to now
 */
static void
tcp_cubic_update_ctime(struct tcpcb *tp)
{
	struct timeval now_timeval;

	getmicrouptime(&now_timeval);
	tp->snd_cubic_ctime = now_timeval.tv_sec * 1000 +
	    now_timeval.tv_usec / 1000;
}

/*
 * miliseconds from last congestion
 */
static uint32_t
tcp_cubic_diff_ctime(struct tcpcb *tp)
{
	struct timeval now_timeval;

	getmicrouptime(&now_timeval);
	return now_timeval.tv_sec * 1000 + now_timeval.tv_usec / 1000 -
	    tp->snd_cubic_ctime;
}

/*
 * Approximate cubic root
 */
#define CBRT_ROUNDS 30
static uint32_t
tcp_cubic_cbrt(uint32_t v)
{
	int i, rounds = CBRT_ROUNDS;
	uint64_t x = v / 3;

	/* We fail to calculate correct for small numbers */
	if (v == 0)
		return 0;
	else if (v < 4)
		return 1;

	/*
	 * largest x that 2*x^3+3*x fits 64bit
	 * Avoid overflow for a time cost
	 */
	if (x > 2097151)
		rounds += 10;

	for (i = 0; i < rounds; i++)
		if (rounds == CBRT_ROUNDS)
			x = (v + 2 * x * x * x) / (3 * x * x);
		else
			/* Avoid overflow */
			x = v / (3 * x * x) + 2 * x / 3;

	return (uint32_t)x;
}

/* Draft Rhee Section 3.1 - get W(t+rtt) - Eq. 1 */
static ulong
tcp_cubic_getW(struct tcpcb *tp, uint32_t ms_elapsed, uint32_t rtt)
{
	uint32_t K;
	long tK3;

	/* Section 3.1 Eq. 2 */
	K = tcp_cubic_cbrt(tp->snd_cubic_wmax / CUBIC_BETAB *
	    CUBIC_CB / CUBIC_CA);
	/*  (t-K)^3 - not clear why is the measure unit mattering */
	tK3 = (long)(ms_elapsed + rtt) - (long)K;
	tK3 = tK3 * tK3 * tK3;

	return CUBIC_CA * tK3 / CUBIC_CB + tp->snd_cubic_wmax;
}

static void
tcp_cubic_congestion_exp(struct tcpcb *tp)
{

	/*
	 * Congestion - Set WMax and shrink cwnd
	 */
	tcp_cubic_update_ctime(tp);

	/* Section 3.6 - Fast Convergence */
	if (tp->snd_cubic_wmax < tp->snd_cubic_wmax_last) {
		tp->snd_cubic_wmax_last = tp->snd_cubic_wmax;
		tp->snd_cubic_wmax = tp->snd_cubic_wmax / 2 +
		    tp->snd_cubic_wmax * CUBIC_BETAA / CUBIC_BETAB / 2;
	} else {
		tp->snd_cubic_wmax_last = tp->snd_cubic_wmax;
		tp->snd_cubic_wmax = tp->snd_cwnd;
	}

	tp->snd_cubic_wmax = max(tp->t_segsz, tp->snd_cubic_wmax);

	/* Shrink CWND */
	tcp_common_congestion_exp(tp, CUBIC_BETAA, CUBIC_BETAB);
}

static int
tcp_cubic_fast_retransmit(struct tcpcb *tp, const struct tcphdr *th)
{

	if (SEQ_LT(th->th_ack, tp->snd_high)) {
		/* See newreno */
		tp->t_dupacks = 0;
		return 1;
	}

	/*
	 * mark WMax
	 */
	tcp_cubic_congestion_exp(tp);

	/* Do fast retransmit */
	return tcp_reno_do_fast_retransmit(tp, th);
}

static void
tcp_cubic_newack(struct tcpcb *tp, const struct tcphdr *th)
{
	uint32_t ms_elapsed, rtt;
	u_long w_tcp;

	/* Congestion avoidance and not in fast recovery and usable rtt */
	if (tp->snd_cwnd > tp->snd_ssthresh && tp->t_partialacks < 0 &&
	    /*
	     * t_srtt is 1/32 units of slow ticks
	     * converting it in ms would be equal to
	     * (t_srtt >> 5) * 1000 / PR_SLOWHZ ~= (t_srtt << 5) / PR_SLOWHZ
	     */
	    (rtt = (tp->t_srtt << 5) / PR_SLOWHZ) > 0) {
		ms_elapsed = tcp_cubic_diff_ctime(tp);

		/* Compute W_tcp(t) */
		w_tcp = tp->snd_cubic_wmax * CUBIC_BETAA / CUBIC_BETAB +
		    ms_elapsed / rtt / 3;

		if (tp->snd_cwnd > w_tcp) {
			/* Not in TCP friendly mode */
			tp->snd_cwnd += (tcp_cubic_getW(tp, ms_elapsed, rtt) -
			    tp->snd_cwnd) / tp->snd_cwnd;
		} else {
			/* friendly TCP mode */
			tp->snd_cwnd = w_tcp;
		}

		/* Make sure we are within limits */
		tp->snd_cwnd = max(tp->snd_cwnd, tp->t_segsz);
		tp->snd_cwnd = min(tp->snd_cwnd, TCP_MAXWIN << tp->snd_scale);
	} else {
		/* Use New Reno */
		tcp_newreno_newack(tp, th);
	}
}

static void
tcp_cubic_slow_retransmit(struct tcpcb *tp)
{

	/* Timeout - Mark new congestion */
	tcp_cubic_congestion_exp(tp);

	/* Loss Window MUST be one segment. */
	tp->snd_cwnd = tp->t_segsz;
	tp->t_partialacks = -1;
	tp->t_dupacks = 0;
	tp->t_bytes_acked = 0;

	if (TCP_ECN_ALLOWED(tp))
		tp->t_flags |= TF_ECN_SND_CWR;
}

const struct tcp_congctl tcp_cubic_ctl = {
	.fast_retransmit = tcp_cubic_fast_retransmit,
	.slow_retransmit = tcp_cubic_slow_retransmit,
	.fast_retransmit_newack = tcp_newreno_fast_retransmit_newack,
	.newack = tcp_cubic_newack,
	.cong_exp = tcp_cubic_congestion_exp,
};
