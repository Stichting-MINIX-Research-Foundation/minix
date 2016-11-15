/*	$NetBSD: tcp_usrreq.c,v 1.209 2015/08/24 22:21:26 pooka Exp $	*/

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

/*-
 * Copyright (c) 1997, 1998, 2005, 2006 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1982, 1986, 1988, 1993, 1995
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
 *	@(#)tcp_usrreq.c	8.5 (Berkeley) 6/21/95
 */

/*
 * TCP protocol interface to socket abstraction.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcp_usrreq.c,v 1.209 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_tcp_debug.h"
#include "opt_mbuftrace.h"
#include "opt_tcp_space.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/uidinfo.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/in_offload.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_private.h>
#include <netinet/tcp_congctl.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>
#include <netinet/tcp_vtw.h>

static int  
tcp_debug_capture(struct tcpcb *tp, int req)  
{
#ifdef KPROF
	tcp_acounts[tp->t_state][req]++;
#endif
#ifdef TCP_DEBUG
	return tp->t_state;
#endif
	return 0;
}

static inline void
tcp_debug_trace(struct socket *so, struct tcpcb *tp, int ostate, int req)
{        
#ifdef TCP_DEBUG
	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, NULL, req);
#endif
}

static int
tcp_getpcb(struct socket *so, struct inpcb **inp,
    struct in6pcb **in6p, struct tcpcb **tp)
{

	KASSERT(solocked(so));

	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidary (struct tcpcb).
	 */
	switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET
	case PF_INET:
		*inp = sotoinpcb(so);
		if (*inp == NULL)
			return EINVAL;
		*tp = intotcpcb(*inp);
		break;
#endif
#ifdef INET6
	case PF_INET6:
		*in6p = sotoin6pcb(so);
		if (*in6p == NULL)
			return EINVAL;
		*tp = in6totcpcb(*in6p);
		break;
#endif
	default:
		return EAFNOSUPPORT;
	}

	KASSERT(tp != NULL);

	return 0;
}

static void
change_keepalive(struct socket *so, struct tcpcb *tp)
{
	tp->t_maxidle = tp->t_keepcnt * tp->t_keepintvl;
	TCP_TIMER_DISARM(tp, TCPT_KEEP);
	TCP_TIMER_DISARM(tp, TCPT_2MSL);

	if (tp->t_state == TCPS_SYN_RECEIVED ||
	    tp->t_state == TCPS_SYN_SENT) {
		TCP_TIMER_ARM(tp, TCPT_KEEP, tp->t_keepinit);
	} else if (so->so_options & SO_KEEPALIVE && 
	    tp->t_state <= TCPS_CLOSE_WAIT) {
		TCP_TIMER_ARM(tp, TCPT_KEEP, tp->t_keepintvl);
	} else {
		TCP_TIMER_ARM(tp, TCPT_KEEP, tp->t_keepidle);
	}

	if ((tp->t_state == TCPS_FIN_WAIT_2) && (tp->t_maxidle > 0))
		TCP_TIMER_ARM(tp, TCPT_2MSL, tp->t_maxidle);
}

/*
 * Export TCP internal state information via a struct tcp_info, based on the
 * Linux 2.6 API.  Not ABI compatible as our constants are mapped differently
 * (TCP state machine, etc).  We export all information using FreeBSD-native
 * constants -- for example, the numeric values for tcpi_state will differ
 * from Linux.
 */
static void
tcp_fill_info(struct tcpcb *tp, struct tcp_info *ti)
{

	bzero(ti, sizeof(*ti));

	ti->tcpi_state = tp->t_state;
	if ((tp->t_flags & TF_REQ_TSTMP) && (tp->t_flags & TF_RCVD_TSTMP))
		ti->tcpi_options |= TCPI_OPT_TIMESTAMPS;
	if (tp->t_flags & TF_SACK_PERMIT)
		ti->tcpi_options |= TCPI_OPT_SACK;
	if ((tp->t_flags & TF_REQ_SCALE) && (tp->t_flags & TF_RCVD_SCALE)) {
		ti->tcpi_options |= TCPI_OPT_WSCALE;
		ti->tcpi_snd_wscale = tp->snd_scale;
		ti->tcpi_rcv_wscale = tp->rcv_scale;
	}
	if (tp->t_flags & TF_ECN_PERMIT) {
		ti->tcpi_options |= TCPI_OPT_ECN;
	}

	ti->tcpi_rto = tp->t_rxtcur * tick;
	ti->tcpi_last_data_recv = (long)(hardclock_ticks -
					 (int)tp->t_rcvtime) * tick;
	ti->tcpi_rtt = ((u_int64_t)tp->t_srtt * tick) >> TCP_RTT_SHIFT;
	ti->tcpi_rttvar = ((u_int64_t)tp->t_rttvar * tick) >> TCP_RTTVAR_SHIFT;

	ti->tcpi_snd_ssthresh = tp->snd_ssthresh;
	/* Linux API wants these in # of segments, apparently */
	ti->tcpi_snd_cwnd = tp->snd_cwnd / tp->t_segsz;
	ti->tcpi_snd_wnd = tp->snd_wnd / tp->t_segsz;

	/*
	 * FreeBSD-specific extension fields for tcp_info.
	 */
	ti->tcpi_rcv_space = tp->rcv_wnd;
	ti->tcpi_rcv_nxt = tp->rcv_nxt;
	ti->tcpi_snd_bwnd = 0;		/* Unused, kept for compat. */
	ti->tcpi_snd_nxt = tp->snd_nxt;
	ti->tcpi_snd_mss = tp->t_segsz;
	ti->tcpi_rcv_mss = tp->t_segsz;
#ifdef TF_TOE
	if (tp->t_flags & TF_TOE)
		ti->tcpi_options |= TCPI_OPT_TOE;
#endif
	/* From the redundant department of redundancies... */
	ti->__tcpi_retransmits = ti->__tcpi_retrans =
		ti->tcpi_snd_rexmitpack = tp->t_sndrexmitpack;

	ti->tcpi_rcv_ooopack = tp->t_rcvoopack;
	ti->tcpi_snd_zerowin = tp->t_sndzerowin;
}

int
tcp_ctloutput(int op, struct socket *so, struct sockopt *sopt)
{
	int error = 0, s;
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	struct tcpcb *tp;
	struct tcp_info ti;
	u_int ui;
	int family;	/* family of the socket */
	int level, optname, optval;

	level = sopt->sopt_level;
	optname = sopt->sopt_name;

	family = so->so_proto->pr_domain->dom_family;

	s = splsoftnet();
	switch (family) {
#ifdef INET
	case PF_INET:
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#endif
#ifdef INET6
	case PF_INET6:
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		splx(s);
		panic("%s: af %d", __func__, family);
	}
#ifndef INET6
	if (inp == NULL)
#else
	if (inp == NULL && in6p == NULL)
#endif
	{
		splx(s);
		return (ECONNRESET);
	}
	if (level != IPPROTO_TCP) {
		switch (family) {
#ifdef INET
		case PF_INET:
			error = ip_ctloutput(op, so, sopt);
			break;
#endif
#ifdef INET6
		case PF_INET6:
			error = ip6_ctloutput(op, so, sopt);
			break;
#endif
		}
		splx(s);
		return (error);
	}
	if (inp)
		tp = intotcpcb(inp);
#ifdef INET6
	else if (in6p)
		tp = in6totcpcb(in6p);
#endif
	else
		tp = NULL;

	switch (op) {
	case PRCO_SETOPT:
		switch (optname) {
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			error = sockopt_getint(sopt, &optval);
			if (error)
				break;
			if (optval > 0)
				tp->t_flags |= TF_SIGNATURE;
			else
				tp->t_flags &= ~TF_SIGNATURE;
			break;
#endif /* TCP_SIGNATURE */

		case TCP_NODELAY:
			error = sockopt_getint(sopt, &optval);
			if (error)
				break;
			if (optval)
				tp->t_flags |= TF_NODELAY;
			else
				tp->t_flags &= ~TF_NODELAY;
			break;

		case TCP_MAXSEG:
			error = sockopt_getint(sopt, &optval);
			if (error)
				break;
			if (optval > 0 && optval <= tp->t_peermss)
				tp->t_peermss = optval; /* limit on send size */
			else
				error = EINVAL;
			break;
#ifdef notyet
		case TCP_CONGCTL:
			/* XXX string overflow XXX */
			error = tcp_congctl_select(tp, sopt->sopt_data);
			break;
#endif

		case TCP_KEEPIDLE:
			error = sockopt_get(sopt, &ui, sizeof(ui));
			if (error)
				break;
			if (ui > 0) {
				tp->t_keepidle = ui;
				change_keepalive(so, tp);
			} else
				error = EINVAL;
			break;

		case TCP_KEEPINTVL:
			error = sockopt_get(sopt, &ui, sizeof(ui));
			if (error)
				break;
			if (ui > 0) {
				tp->t_keepintvl = ui;
				change_keepalive(so, tp);
			} else
				error = EINVAL;
			break;

		case TCP_KEEPCNT:
			error = sockopt_get(sopt, &ui, sizeof(ui));
			if (error)
				break;
			if (ui > 0) {
				tp->t_keepcnt = ui;
				change_keepalive(so, tp);
			} else
				error = EINVAL;
			break;

		case TCP_KEEPINIT:
			error = sockopt_get(sopt, &ui, sizeof(ui));
			if (error)
				break;
			if (ui > 0) {
				tp->t_keepinit = ui;
				change_keepalive(so, tp);
			} else
				error = EINVAL;
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case PRCO_GETOPT:
		switch (optname) {
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			optval = (tp->t_flags & TF_SIGNATURE) ? 1 : 0;
			error = sockopt_set(sopt, &optval, sizeof(optval));
			break;
#endif
		case TCP_NODELAY:
			optval = tp->t_flags & TF_NODELAY;
			error = sockopt_set(sopt, &optval, sizeof(optval));
			break;
		case TCP_MAXSEG:
			optval = tp->t_peermss;
			error = sockopt_set(sopt, &optval, sizeof(optval));
			break;
		case TCP_INFO:
			tcp_fill_info(tp, &ti);
			error = sockopt_set(sopt, &ti, sizeof ti);
			break;
#ifdef notyet
		case TCP_CONGCTL:
			break;
#endif
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	splx(s);
	return (error);
}

#ifndef TCP_SENDSPACE
#define	TCP_SENDSPACE	1024*32
#endif
int	tcp_sendspace = TCP_SENDSPACE;
#ifndef TCP_RECVSPACE
#define	TCP_RECVSPACE	1024*32
#endif
int	tcp_recvspace = TCP_RECVSPACE;

/*
 * tcp_attach: attach TCP protocol to socket, allocating internet protocol
 * control block, TCP control block, buffer space and entering LISTEN state
 * if to accept connections.
 */
static int
tcp_attach(struct socket *so, int proto)
{
	struct tcpcb *tp;
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	int s, error, family;

	/* Assign the lock (must happen even if we will error out). */
	s = splsoftnet();
	sosetlock(so);
	KASSERT(solocked(so));

	family = so->so_proto->pr_domain->dom_family;
	switch (family) {
#ifdef INET
	case PF_INET:
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#endif
#ifdef INET6
	case PF_INET6:
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		goto out;
	}

	KASSERT(inp == NULL);
#ifdef INET6
	KASSERT(in6p == NULL);
#endif

#ifdef MBUFTRACE
	so->so_mowner = &tcp_sock_mowner;
	so->so_rcv.sb_mowner = &tcp_sock_rx_mowner;
	so->so_snd.sb_mowner = &tcp_sock_tx_mowner;
#endif
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, tcp_sendspace, tcp_recvspace);
		if (error)
			goto out;
	}

	so->so_rcv.sb_flags |= SB_AUTOSIZE;
	so->so_snd.sb_flags |= SB_AUTOSIZE;

	switch (family) {
#ifdef INET
	case PF_INET:
		error = in_pcballoc(so, &tcbtable);
		if (error)
			goto out;
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#endif
#ifdef INET6
	case PF_INET6:
		error = in6_pcballoc(so, &tcbtable);
		if (error)
			goto out;
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		goto out;
	}
	if (inp)
		tp = tcp_newtcpcb(family, (void *)inp);
#ifdef INET6
	else if (in6p)
		tp = tcp_newtcpcb(family, (void *)in6p);
#endif
	else
		tp = NULL;

	if (tp == NULL) {
		int nofd = so->so_state & SS_NOFDREF;	/* XXX */

		so->so_state &= ~SS_NOFDREF;	/* don't free the socket yet */
#ifdef INET
		if (inp)
			in_pcbdetach(inp);
#endif
#ifdef INET6
		if (in6p)
			in6_pcbdetach(in6p);
#endif
		so->so_state |= nofd;
		error = ENOBUFS;
		goto out;
	}
	tp->t_state = TCPS_CLOSED;
	if ((so->so_options & SO_LINGER) && so->so_linger == 0) {
		so->so_linger = TCP_LINGERTIME;
	}
out:
	KASSERT(solocked(so));
	splx(s);
	return error;
}

static void
tcp_detach(struct socket *so)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int s;

	if (tcp_getpcb(so, &inp, &in6p, &tp) != 0)
		return;

	s = splsoftnet();
	(void)tcp_disconnect1(tp);
	splx(s);
}

static int
tcp_accept(struct socket *so, struct sockaddr *nam)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int ostate = 0;
	int error = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_ACCEPT);

	/*
	 * Accept a connection.  Essentially all the work is
	 * done at higher levels; just return the address
	 * of the peer, storing through addr.
	 */
	s = splsoftnet();
#ifdef INET
	if (inp) {
		in_setpeeraddr(inp, (struct sockaddr_in *)nam);
	}
#endif
#ifdef INET6
	if (in6p) {
		in6_setpeeraddr(in6p, (struct sockaddr_in6 *)nam);
	}
#endif
	tcp_debug_trace(so, tp, ostate, PRU_ACCEPT);
	splx(s);

	return 0;
}

static int
tcp_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;
#ifdef INET6
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
#endif /* INET6 */
	struct tcpcb *tp = NULL;
	int s;
	int error = 0;
	int ostate = 0;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_BIND);

	/*
	 * Give the socket an address.
	 */
	s = splsoftnet();
	switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET
	case PF_INET:
		error = in_pcbbind(inp, sin, l);
		break;
#endif
#ifdef INET6
	case PF_INET6:
		error = in6_pcbbind(in6p, sin6, l);
		if (!error) {
			/* mapped addr case */
			if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr))
				tp->t_family = AF_INET;
			else
				tp->t_family = AF_INET6;
		}
		break;
#endif
	}
	tcp_debug_trace(so, tp, ostate, PRU_BIND);
	splx(s);

	return error;
}

static int
tcp_listen(struct socket *so, struct lwp *l)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int error = 0;
	int ostate = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_LISTEN);

	/*
	 * Prepare to accept connections.
	 */
	s = splsoftnet();
#ifdef INET
	if (inp && inp->inp_lport == 0) {
		error = in_pcbbind(inp, NULL, l);
		if (error)
			goto release;
	}
#endif
#ifdef INET6
	if (in6p && in6p->in6p_lport == 0) {
		error = in6_pcbbind(in6p, NULL, l);
		if (error)
			goto release;
	}
#endif
	tp->t_state = TCPS_LISTEN;

release:
	tcp_debug_trace(so, tp, ostate, PRU_LISTEN);
	splx(s);

	return error;
}

static int
tcp_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int s;
	int error = 0;
	int ostate = 0;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_CONNECT);

	/*
	 * Initiate connection to peer.
	 * Create a template for use in transmissions on this connection.
	 * Enter SYN_SENT state, and mark socket as connecting.
	 * Start keep-alive timer, and seed output sequence space.
	 * Send initial segment on connection.
	 */
	s = splsoftnet();
#ifdef INET
	if (inp) {
		if (inp->inp_lport == 0) {
			error = in_pcbbind(inp, NULL, l);
			if (error)
				goto release;
		}
		error = in_pcbconnect(inp, (struct sockaddr_in *)nam, l);
	}
#endif
#ifdef INET6
	if (in6p) {
		if (in6p->in6p_lport == 0) {
			error = in6_pcbbind(in6p, NULL, l);
			if (error)
				goto release;
		}
		error = in6_pcbconnect(in6p, (struct sockaddr_in6 *)nam, l);
		if (!error) {
			/* mapped addr case */
			if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr))
				tp->t_family = AF_INET;
			else
				tp->t_family = AF_INET6;
		}
	}
#endif
	if (error)
		goto release;
	tp->t_template = tcp_template(tp);
	if (tp->t_template == 0) {
#ifdef INET
		if (inp)
			in_pcbdisconnect(inp);
#endif
#ifdef INET6
		if (in6p)
			in6_pcbdisconnect(in6p);
#endif
		error = ENOBUFS;
		goto release;
	}
	/*
	 * Compute window scaling to request.
	 * XXX: This should be moved to tcp_output().
	 */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < sb_max)
		tp->request_r_scale++;
	soisconnecting(so);
	TCP_STATINC(TCP_STAT_CONNATTEMPT);
	tp->t_state = TCPS_SYN_SENT;
	TCP_TIMER_ARM(tp, TCPT_KEEP, tp->t_keepinit);
	tp->iss = tcp_new_iss(tp, 0);
	tcp_sendseqinit(tp);
	error = tcp_output(tp);

release:
	tcp_debug_trace(so, tp, ostate, PRU_CONNECT);
	splx(s);

	return error;
}

static int
tcp_connect2(struct socket *so, struct socket *so2)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int error = 0;
	int ostate = 0;

	KASSERT(solocked(so));

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_CONNECT2);

	tcp_debug_trace(so, tp, ostate, PRU_CONNECT2);

	return EOPNOTSUPP;
}

static int
tcp_disconnect(struct socket *so)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int error = 0;
	int ostate = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_DISCONNECT);

	/*
	 * Initiate disconnect from peer.
	 * If connection never passed embryonic stage, just drop;
	 * else if don't need to let data drain, then can just drop anyways,
	 * else have to begin TCP shutdown process: mark socket disconnecting,
	 * drain unread data, state switch to reflect user close, and
	 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
	 * when peer sends FIN and acks ours.
	 *
	 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
	 */
	s = splsoftnet();
	tp = tcp_disconnect1(tp);
	tcp_debug_trace(so, tp, ostate, PRU_DISCONNECT);
	splx(s);

	return error;
}

static int
tcp_shutdown(struct socket *so)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int error = 0;
	int ostate = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_SHUTDOWN);
	/*
	 * Mark the connection as being incapable of further output.
	 */
	s = splsoftnet();
	socantsendmore(so);
	tp = tcp_usrclosed(tp);
	if (tp)
		error = tcp_output(tp);
	tcp_debug_trace(so, tp, ostate, PRU_SHUTDOWN);
	splx(s);

	return error;
}

static int
tcp_abort(struct socket *so)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int error = 0;
	int ostate = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_ABORT);

	/*
	 * Abort the TCP.
	 */
	s = splsoftnet();
	tp = tcp_drop(tp, ECONNABORTED);
	tcp_debug_trace(so, tp, ostate, PRU_ABORT);
	splx(s);

	return error;
}

static int
tcp_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET
	case PF_INET:
		return in_control(so, cmd, nam, ifp);
#endif
#ifdef INET6
	case PF_INET6:
		return in6_control(so, cmd, nam, ifp);
#endif
	default:
		return EAFNOSUPPORT;
	}
}

static int
tcp_stat(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	/* stat: don't bother with a blocksize.  */
	return 0;
}

static int
tcp_peeraddr(struct socket *so, struct sockaddr *nam)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int ostate = 0;
	int error = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_PEERADDR);

	s = splsoftnet();
#ifdef INET
	if (inp) {
		in_setpeeraddr(inp, (struct sockaddr_in *)nam);
	}
#endif
#ifdef INET6
	if (in6p) {
		in6_setpeeraddr(in6p, (struct sockaddr_in6 *)nam);
	}
#endif
	tcp_debug_trace(so, tp, ostate, PRU_PEERADDR);
	splx(s);

	return 0;
}

static int
tcp_sockaddr(struct socket *so, struct sockaddr *nam)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int ostate = 0;
	int error = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_SOCKADDR);

	s = splsoftnet();
#ifdef INET
	if (inp) {
		in_setsockaddr(inp, (struct sockaddr_in *)nam);
	}
#endif
#ifdef INET6
	if (in6p) {
		in6_setsockaddr(in6p, (struct sockaddr_in6 *)nam);
	}
#endif
	tcp_debug_trace(so, tp, ostate, PRU_SOCKADDR);
	splx(s);

	return 0;
}

static int
tcp_rcvd(struct socket *so, int flags, struct lwp *l)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int ostate = 0;
	int error = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_RCVD);

	/*
	 * After a receive, possibly send window update to peer.
	 *
	 * soreceive() calls this function when a user receives
	 * ancillary data on a listening socket. We don't call
	 * tcp_output in such a case, since there is no header
	 * template for a listening socket and hence the kernel
	 * will panic.
	 */
	s = splsoftnet();
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) != 0)
		(void) tcp_output(tp);
	splx(s);

	tcp_debug_trace(so, tp, ostate, PRU_RCVD);

	return 0;
}

static int
tcp_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int ostate = 0;
	int error = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_RCVOOB);

	s = splsoftnet();
	if ((so->so_oobmark == 0 &&
	    (so->so_state & SS_RCVATMARK) == 0) ||
	    so->so_options & SO_OOBINLINE ||
	    tp->t_oobflags & TCPOOB_HADDATA) {
		splx(s);
		return EINVAL;
	}

	if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
		splx(s);
		return EWOULDBLOCK;
	}

	m->m_len = 1;
	*mtod(m, char *) = tp->t_iobc;
	if ((flags & MSG_PEEK) == 0)
		tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);

	tcp_debug_trace(so, tp, ostate, PRU_RCVOOB);
	splx(s);

	return 0;
}

static int
tcp_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int ostate = 0;
	int error = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_SEND);

	/*
	 * Do a send by putting data in output queue and updating urgent
	 * marker if URG set.  Possibly send more data.
	 */
	s = splsoftnet();
	if (control && control->m_len) {
		m_freem(control);
		m_freem(m);
		tcp_debug_trace(so, tp, ostate, PRU_SEND);
		splx(s);
		return EINVAL;
	}

	sbappendstream(&so->so_snd, m);
	error = tcp_output(tp);
	tcp_debug_trace(so, tp, ostate, PRU_SEND);
	splx(s);

	return error;
}

static int
tcp_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	struct inpcb *inp = NULL;
	struct in6pcb *in6p = NULL;
	struct tcpcb *tp = NULL;
	int ostate = 0;
	int error = 0;
	int s;

	if ((error = tcp_getpcb(so, &inp, &in6p, &tp)) != 0)
		return error;

	ostate = tcp_debug_capture(tp, PRU_SENDOOB);

	s = splsoftnet();
	if (sbspace(&so->so_snd) < -512) {
		m_freem(m);
		splx(s);
		return ENOBUFS;
	}
	/*
	 * According to RFC961 (Assigned Protocols),
	 * the urgent pointer points to the last octet
	 * of urgent data.  We continue, however,
	 * to consider it to indicate the first octet
	 * of data past the urgent section.
	 * Otherwise, snd_up should be one lower.
	 */
	sbappendstream(&so->so_snd, m);
	tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
	tp->t_force = 1;
	error = tcp_output(tp);
	tp->t_force = 0;
	tcp_debug_trace(so, tp, ostate, PRU_SENDOOB);
	splx(s);

	return error;
}

static int
tcp_purgeif(struct socket *so, struct ifnet *ifp)
{
	int s;

	s = splsoftnet();
	mutex_enter(softnet_lock);
	switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET
	case PF_INET:
		in_pcbpurgeif0(&tcbtable, ifp);
		in_purgeif(ifp);
		in_pcbpurgeif(&tcbtable, ifp);
		break;
#endif
#ifdef INET6
	case PF_INET6:
		in6_pcbpurgeif0(&tcbtable, ifp);
		in6_purgeif(ifp);
		in6_pcbpurgeif(&tcbtable, ifp);
		break;
#endif
	default:
		mutex_exit(softnet_lock);
		splx(s);
		return EAFNOSUPPORT;
	}
	mutex_exit(softnet_lock);
	splx(s);

	return 0;
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
struct tcpcb *
tcp_disconnect1(struct tcpcb *tp)
{
	struct socket *so;

	if (tp->t_inpcb)
		so = tp->t_inpcb->inp_socket;
#ifdef INET6
	else if (tp->t_in6pcb)
		so = tp->t_in6pcb->in6p_socket;
#endif
	else
		so = NULL;

	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			(void) tcp_output(tp);
	}
	return (tp);
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
struct tcpcb *
tcp_usrclosed(struct tcpcb *tp)
{

	switch (tp->t_state) {

	case TCPS_CLOSED:
	case TCPS_LISTEN:
	case TCPS_SYN_SENT:
		tp->t_state = TCPS_CLOSED;
		tp = tcp_close(tp);
		break;

	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;

	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}
	if (tp && tp->t_state >= TCPS_FIN_WAIT_2) {
		struct socket *so;
		if (tp->t_inpcb)
			so = tp->t_inpcb->inp_socket;
#ifdef INET6
		else if (tp->t_in6pcb)
			so = tp->t_in6pcb->in6p_socket;
#endif
		else
			so = NULL;
		if (so)
			soisdisconnected(so);
		/*
		 * If we are in FIN_WAIT_2, we arrived here because the
		 * application did a shutdown of the send side.  Like the
		 * case of a transition from FIN_WAIT_1 to FIN_WAIT_2 after
		 * a full close, we start a timer to make sure sockets are
		 * not left in FIN_WAIT_2 forever.
		 */
		if ((tp->t_state == TCPS_FIN_WAIT_2) && (tp->t_maxidle > 0))
			TCP_TIMER_ARM(tp, TCPT_2MSL, tp->t_maxidle);
		else if (tp->t_state == TCPS_TIME_WAIT
			 && ((tp->t_inpcb
			      && (tcp4_vtw_enable & 1)
			      && vtw_add(AF_INET, tp))
			     ||
			     (tp->t_in6pcb
			      && (tcp6_vtw_enable & 1)
			      && vtw_add(AF_INET6, tp)))) {
			tp = 0;
		}
	}
	return (tp);
}

/*
 * sysctl helper routine for net.inet.ip.mssdflt.  it can't be less
 * than 32.
 */
static int
sysctl_net_inet_tcp_mssdflt(SYSCTLFN_ARGS)
{
	int error, mssdflt;
	struct sysctlnode node;

	mssdflt = tcp_mssdflt;
	node = *rnode;
	node.sysctl_data = &mssdflt;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (mssdflt < 32)
		return (EINVAL);
	tcp_mssdflt = mssdflt;

	mutex_enter(softnet_lock);
	tcp_tcpcb_template();
	mutex_exit(softnet_lock);

	return (0);
}

/*
 * sysctl helper for TCP CB template update
 */
static int
sysctl_update_tcpcb_template(SYSCTLFN_ARGS)
{
	int t, error;
	struct sysctlnode node;

	/* follow procedures in sysctl(9) manpage */
	t = *(int *)rnode->sysctl_data;
	node = *rnode;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < 0)
		return EINVAL;

	*(int *)rnode->sysctl_data = t;

	mutex_enter(softnet_lock);
	tcp_tcpcb_template();
	mutex_exit(softnet_lock);

	return 0;
}

/*
 * sysctl helper routine for setting port related values under
 * net.inet.ip and net.inet6.ip6.  does basic range checking and does
 * additional checks for each type.  this code has placed in
 * tcp_input.c since INET and INET6 both use the same tcp code.
 *
 * this helper is not static so that both inet and inet6 can use it.
 */
int
sysctl_net_inet_ip_ports(SYSCTLFN_ARGS)
{
	int error, tmp;
	int apmin, apmax;
#ifndef IPNOPRIVPORTS
	int lpmin, lpmax;
#endif /* IPNOPRIVPORTS */
	struct sysctlnode node;

	if (namelen != 0)
		return (EINVAL);

	switch (name[-3]) {
#ifdef INET
	    case PF_INET:
		apmin = anonportmin;
		apmax = anonportmax;
#ifndef IPNOPRIVPORTS
		lpmin = lowportmin;
		lpmax = lowportmax;
#endif /* IPNOPRIVPORTS */
		break;
#endif /* INET */
#ifdef INET6
	    case PF_INET6:
		apmin = ip6_anonportmin;
		apmax = ip6_anonportmax;
#ifndef IPNOPRIVPORTS
		lpmin = ip6_lowportmin;
		lpmax = ip6_lowportmax;
#endif /* IPNOPRIVPORTS */
		break;
#endif /* INET6 */
	    default:
		return (EINVAL);
	}

	/*
	 * insert temporary copy into node, perform lookup on
	 * temporary, then restore pointer
	 */
	node = *rnode;
	tmp = *(int*)rnode->sysctl_data;
	node.sysctl_data = &tmp;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	/*
	 * simple port range check
	 */
	if (tmp < 0 || tmp > 65535)
		return (EINVAL);

	/*
	 * per-node range checks
	 */
	switch (rnode->sysctl_num) {
	case IPCTL_ANONPORTMIN:
	case IPV6CTL_ANONPORTMIN:
		if (tmp >= apmax)
			return (EINVAL);
#ifndef IPNOPRIVPORTS
		if (tmp < IPPORT_RESERVED)
                        return (EINVAL);
#endif /* IPNOPRIVPORTS */
		break;

	case IPCTL_ANONPORTMAX:
	case IPV6CTL_ANONPORTMAX:
                if (apmin >= tmp)
			return (EINVAL);
#ifndef IPNOPRIVPORTS
		if (tmp < IPPORT_RESERVED)
                        return (EINVAL);
#endif /* IPNOPRIVPORTS */
		break;

#ifndef IPNOPRIVPORTS
	case IPCTL_LOWPORTMIN:
	case IPV6CTL_LOWPORTMIN:
		if (tmp >= lpmax ||
		    tmp > IPPORT_RESERVEDMAX ||
		    tmp < IPPORT_RESERVEDMIN)
			return (EINVAL);
		break;

	case IPCTL_LOWPORTMAX:
	case IPV6CTL_LOWPORTMAX:
		if (lpmin >= tmp ||
		    tmp > IPPORT_RESERVEDMAX ||
		    tmp < IPPORT_RESERVEDMIN)
			return (EINVAL);
		break;
#endif /* IPNOPRIVPORTS */

	default:
		return (EINVAL);
	}

	*(int*)rnode->sysctl_data = tmp;

	return (0);
}

static inline int
copyout_uid(struct socket *sockp, void *oldp, size_t *oldlenp)
{
	if (oldp) {
		size_t sz;
		uid_t uid;
		int error;

		if (sockp->so_cred == NULL)
			return EPERM;

		uid = kauth_cred_geteuid(sockp->so_cred);
		sz = MIN(sizeof(uid), *oldlenp);
		if ((error = copyout(&uid, oldp, sz)) != 0)
			return error;
	}
	*oldlenp = sizeof(uid_t);
	return 0;
}

static inline int
inet4_ident_core(struct in_addr raddr, u_int rport,
    struct in_addr laddr, u_int lport,
    void *oldp, size_t *oldlenp,
    struct lwp *l, int dodrop)
{
	struct inpcb *inp;
	struct socket *sockp;

	inp = in_pcblookup_connect(&tcbtable, raddr, rport, laddr, lport, 0);
	
	if (inp == NULL || (sockp = inp->inp_socket) == NULL)
		return ESRCH;

	if (dodrop) {
		struct tcpcb *tp;
		int error;
		
		if (inp == NULL || (tp = intotcpcb(inp)) == NULL ||
		    (inp->inp_socket->so_options & SO_ACCEPTCONN) != 0)
			return ESRCH;

		error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_SOCKET,
		    KAUTH_REQ_NETWORK_SOCKET_DROP, inp->inp_socket, tp, NULL);
		if (error)
			return (error);
		
		(void)tcp_drop(tp, ECONNABORTED);
		return 0;
	}
	else
		return copyout_uid(sockp, oldp, oldlenp);
}

#ifdef INET6
static inline int
inet6_ident_core(struct in6_addr *raddr, u_int rport,
    struct in6_addr *laddr, u_int lport,
    void *oldp, size_t *oldlenp,
    struct lwp *l, int dodrop)
{
	struct in6pcb *in6p;
	struct socket *sockp;

	in6p = in6_pcblookup_connect(&tcbtable, raddr, rport, laddr, lport, 0, 0);

	if (in6p == NULL || (sockp = in6p->in6p_socket) == NULL)
		return ESRCH;
	
	if (dodrop) {
		struct tcpcb *tp;
		int error;
		
		if (in6p == NULL || (tp = in6totcpcb(in6p)) == NULL ||
		    (in6p->in6p_socket->so_options & SO_ACCEPTCONN) != 0)
			return ESRCH;

		error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_SOCKET,
		    KAUTH_REQ_NETWORK_SOCKET_DROP, in6p->in6p_socket, tp, NULL);
		if (error)
			return (error);

		(void)tcp_drop(tp, ECONNABORTED);
		return 0;
	}
	else
		return copyout_uid(sockp, oldp, oldlenp);
}
#endif

/*
 * sysctl helper routine for the net.inet.tcp.drop and
 * net.inet6.tcp6.drop nodes.
 */
#define sysctl_net_inet_tcp_drop sysctl_net_inet_tcp_ident

/*
 * sysctl helper routine for the net.inet.tcp.ident and
 * net.inet6.tcp6.ident nodes.  contains backwards compat code for the
 * old way of looking up the ident information for ipv4 which involves
 * stuffing the port/addr pairs into the mib lookup.
 */
static int
sysctl_net_inet_tcp_ident(SYSCTLFN_ARGS)
{
#ifdef INET
	struct sockaddr_in *si4[2];
#endif /* INET */
#ifdef INET6
	struct sockaddr_in6 *si6[2];
#endif /* INET6 */
	struct sockaddr_storage sa[2];
	int error, pf, dodrop;

	dodrop = name[-1] == TCPCTL_DROP;
	if (dodrop) {
		if (oldp != NULL || *oldlenp != 0)
			return EINVAL;
		if (newp == NULL)
			return EPERM;
		if (newlen < sizeof(sa))
			return ENOMEM;
	}
	if (namelen != 4 && namelen != 0)
		return EINVAL;
	if (name[-2] != IPPROTO_TCP)
		return EINVAL;
	pf = name[-3];

	/* old style lookup, ipv4 only */
	if (namelen == 4) {
#ifdef INET
		struct in_addr laddr, raddr;
		u_int lport, rport;

		if (pf != PF_INET)
			return EPROTONOSUPPORT;
		raddr.s_addr = (uint32_t)name[0];
		rport = (u_int)name[1];
		laddr.s_addr = (uint32_t)name[2];
		lport = (u_int)name[3];
		
		mutex_enter(softnet_lock);
		error = inet4_ident_core(raddr, rport, laddr, lport,
		    oldp, oldlenp, l, dodrop);
		mutex_exit(softnet_lock);
		return error;
#else /* INET */
		return EINVAL;
#endif /* INET */
	}

	if (newp == NULL || newlen != sizeof(sa))
		return EINVAL;
	error = copyin(newp, &sa, newlen);
	if (error)
		return error;

	/*
	 * requested families must match
	 */
	if (pf != sa[0].ss_family || sa[0].ss_family != sa[1].ss_family)
		return EINVAL;

	switch (pf) {
#ifdef INET6
	case PF_INET6:
		si6[0] = (struct sockaddr_in6*)&sa[0];
		si6[1] = (struct sockaddr_in6*)&sa[1];
		if (si6[0]->sin6_len != sizeof(*si6[0]) ||
		    si6[1]->sin6_len != sizeof(*si6[1]))
			return EINVAL;

		if (!IN6_IS_ADDR_V4MAPPED(&si6[0]->sin6_addr) &&
		    !IN6_IS_ADDR_V4MAPPED(&si6[1]->sin6_addr)) {
			error = sa6_embedscope(si6[0], ip6_use_defzone);
			if (error)
				return error;
			error = sa6_embedscope(si6[1], ip6_use_defzone);
			if (error)
				return error;

			mutex_enter(softnet_lock);
			error = inet6_ident_core(&si6[0]->sin6_addr,
			    si6[0]->sin6_port, &si6[1]->sin6_addr,
			    si6[1]->sin6_port, oldp, oldlenp, l, dodrop);
			mutex_exit(softnet_lock);
			return error;
		}

		if (IN6_IS_ADDR_V4MAPPED(&si6[0]->sin6_addr) !=
		    IN6_IS_ADDR_V4MAPPED(&si6[1]->sin6_addr))
			return EINVAL;

		in6_sin6_2_sin_in_sock((struct sockaddr *)&sa[0]);
		in6_sin6_2_sin_in_sock((struct sockaddr *)&sa[1]);
		/*FALLTHROUGH*/
#endif /* INET6 */
#ifdef INET
	case PF_INET:
		si4[0] = (struct sockaddr_in*)&sa[0];
		si4[1] = (struct sockaddr_in*)&sa[1];
		if (si4[0]->sin_len != sizeof(*si4[0]) ||
		    si4[0]->sin_len != sizeof(*si4[1]))
			return EINVAL;
	
		mutex_enter(softnet_lock);
		error = inet4_ident_core(si4[0]->sin_addr, si4[0]->sin_port,
		    si4[1]->sin_addr, si4[1]->sin_port,
		    oldp, oldlenp, l, dodrop);
		mutex_exit(softnet_lock);
		return error;
#endif /* INET */
	default:
		return EPROTONOSUPPORT;
	}
}

/*
 * sysctl helper for the inet and inet6 pcblists.  handles tcp/udp and
 * inet/inet6, as well as raw pcbs for each.  specifically not
 * declared static so that raw sockets and udp/udp6 can use it as
 * well.
 */
int
sysctl_inpcblist(SYSCTLFN_ARGS)
{
#ifdef INET
	struct sockaddr_in *in;
	const struct inpcb *inp;
#endif
#ifdef INET6
	struct sockaddr_in6 *in6;
	const struct in6pcb *in6p;
#endif
	struct inpcbtable *pcbtbl = __UNCONST(rnode->sysctl_data);
	const struct inpcb_hdr *inph;
	struct tcpcb *tp;
	struct kinfo_pcb pcb;
	char *dp;
	size_t len, needed, elem_size, out_size;
	int error, elem_count, pf, proto, pf2;

	if (namelen != 4)
		return (EINVAL);

	if (oldp != NULL) {
		    len = *oldlenp;
		    elem_size = name[2];
		    elem_count = name[3];
		    if (elem_size != sizeof(pcb))
			    return EINVAL;
	} else {
		    len = 0;
		    elem_count = INT_MAX;
		    elem_size = sizeof(pcb);
	}
	error = 0;
	dp = oldp;
	out_size = elem_size;
	needed = 0;

	if (namelen == 1 && name[0] == CTL_QUERY)
		return (sysctl_query(SYSCTLFN_CALL(rnode)));

	if (name - oname != 4)
		return (EINVAL);

	pf = oname[1];
	proto = oname[2];
	pf2 = (oldp != NULL) ? pf : 0;

	mutex_enter(softnet_lock);

	TAILQ_FOREACH(inph, &pcbtbl->inpt_queue, inph_queue) {
#ifdef INET
		inp = (const struct inpcb *)inph;
#endif
#ifdef INET6
		in6p = (const struct in6pcb *)inph;
#endif

		if (inph->inph_af != pf)
			continue;

		if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_SOCKET,
		    KAUTH_REQ_NETWORK_SOCKET_CANSEE, inph->inph_socket, NULL,
		    NULL) != 0)
			continue;

		memset(&pcb, 0, sizeof(pcb));

		pcb.ki_family = pf;
		pcb.ki_type = proto;

		switch (pf2) {
		case 0:
			/* just probing for size */
			break;
#ifdef INET
		case PF_INET:
			pcb.ki_family = inp->inp_socket->so_proto->
			    pr_domain->dom_family;
			pcb.ki_type = inp->inp_socket->so_proto->
			    pr_type;
			pcb.ki_protocol = inp->inp_socket->so_proto->
			    pr_protocol;
			pcb.ki_pflags = inp->inp_flags;

			pcb.ki_sostate = inp->inp_socket->so_state;
			pcb.ki_prstate = inp->inp_state;
			if (proto == IPPROTO_TCP) {
				tp = intotcpcb(inp);
				pcb.ki_tstate = tp->t_state;
				pcb.ki_tflags = tp->t_flags;
			}

			pcb.ki_pcbaddr = PTRTOUINT64(inp);
			pcb.ki_ppcbaddr = PTRTOUINT64(inp->inp_ppcb);
			pcb.ki_sockaddr = PTRTOUINT64(inp->inp_socket);

			pcb.ki_rcvq = inp->inp_socket->so_rcv.sb_cc;
			pcb.ki_sndq = inp->inp_socket->so_snd.sb_cc;

			in = satosin(&pcb.ki_src);
			in->sin_len = sizeof(*in);
			in->sin_family = pf;
			in->sin_port = inp->inp_lport;
			in->sin_addr = inp->inp_laddr;
			if (pcb.ki_prstate >= INP_CONNECTED) {
				in = satosin(&pcb.ki_dst);
				in->sin_len = sizeof(*in);
				in->sin_family = pf;
				in->sin_port = inp->inp_fport;
				in->sin_addr = inp->inp_faddr;
			}
			break;
#endif
#ifdef INET6
		case PF_INET6:
			pcb.ki_family = in6p->in6p_socket->so_proto->
			    pr_domain->dom_family;
			pcb.ki_type = in6p->in6p_socket->so_proto->pr_type;
			pcb.ki_protocol = in6p->in6p_socket->so_proto->
			    pr_protocol;
			pcb.ki_pflags = in6p->in6p_flags;

			pcb.ki_sostate = in6p->in6p_socket->so_state;
			pcb.ki_prstate = in6p->in6p_state;
			if (proto == IPPROTO_TCP) {
				tp = in6totcpcb(in6p);
				pcb.ki_tstate = tp->t_state;
				pcb.ki_tflags = tp->t_flags;
			}

			pcb.ki_pcbaddr = PTRTOUINT64(in6p);
			pcb.ki_ppcbaddr = PTRTOUINT64(in6p->in6p_ppcb);
			pcb.ki_sockaddr = PTRTOUINT64(in6p->in6p_socket);

			pcb.ki_rcvq = in6p->in6p_socket->so_rcv.sb_cc;
			pcb.ki_sndq = in6p->in6p_socket->so_snd.sb_cc;

			in6 = satosin6(&pcb.ki_src);
			in6->sin6_len = sizeof(*in6);
			in6->sin6_family = pf;
			in6->sin6_port = in6p->in6p_lport;
			in6->sin6_flowinfo = in6p->in6p_flowinfo;
			in6->sin6_addr = in6p->in6p_laddr;
			in6->sin6_scope_id = 0; /* XXX? */

			if (pcb.ki_prstate >= IN6P_CONNECTED) {
				in6 = satosin6(&pcb.ki_dst);
				in6->sin6_len = sizeof(*in6);
				in6->sin6_family = pf;
				in6->sin6_port = in6p->in6p_fport;
				in6->sin6_flowinfo = in6p->in6p_flowinfo;
				in6->sin6_addr = in6p->in6p_faddr;
				in6->sin6_scope_id = 0; /* XXX? */
			}
			break;
#endif
		}

		if (len >= elem_size && elem_count > 0) {
			error = copyout(&pcb, dp, out_size);
			if (error) {
				mutex_exit(softnet_lock);
				return (error);
			}
			dp += elem_size;
			len -= elem_size;
		}
		needed += elem_size;
		if (elem_count > 0 && elem_count != INT_MAX)
			elem_count--;
	}

	*oldlenp = needed;
	if (oldp == NULL)
		*oldlenp += PCB_SLOP * sizeof(struct kinfo_pcb);

	mutex_exit(softnet_lock);

	return (error);
}

static int
sysctl_tcp_congctl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int error;
	char newname[TCPCC_MAXLEN];

	strlcpy(newname, tcp_congctl_global_name, sizeof(newname) - 1);
	
	node = *rnode;
	node.sysctl_data = newname;
	node.sysctl_size = sizeof(newname);

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	
	if (error || 
	    newp == NULL ||
	    strncmp(newname, tcp_congctl_global_name, sizeof(newname)) == 0)
		return error;

	mutex_enter(softnet_lock);
	error = tcp_congctl_select(NULL, newname);
	mutex_exit(softnet_lock);

	return error;
}

static int
sysctl_tcp_init_win(SYSCTLFN_ARGS)
{
	int error;
	u_int iw;
	struct sysctlnode node;

	iw = *(u_int *)rnode->sysctl_data;
	node = *rnode;
	node.sysctl_data = &iw;
	node.sysctl_size = sizeof(iw);
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (iw >= __arraycount(tcp_init_win_max))
		return EINVAL;
	*(u_int *)rnode->sysctl_data = iw;
	return 0;
}

static int
sysctl_tcp_keep(SYSCTLFN_ARGS)
{  
	int error;
	u_int tmp;
	struct sysctlnode node;

	node = *rnode;
	tmp = *(u_int *)rnode->sysctl_data;
	node.sysctl_data = &tmp;

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	mutex_enter(softnet_lock);

	*(u_int *)rnode->sysctl_data = tmp;
	tcp_tcpcb_template();	/* update the template */

	mutex_exit(softnet_lock);
	return 0;
}

static int
sysctl_net_inet_tcp_stats(SYSCTLFN_ARGS)
{

	return (NETSTAT_SYSCTL(tcpstat_percpu, TCP_NSTATS));
}

/*
 * this (second stage) setup routine is a replacement for tcp_sysctl()
 * (which is currently used for ipv4 and ipv6)
 */
static void
sysctl_net_inet_tcp_setup2(struct sysctllog **clog, int pf, const char *pfname,
			   const char *tcpname)
{
	const struct sysctlnode *sack_node;
	const struct sysctlnode *abc_node;
	const struct sysctlnode *ecn_node;
	const struct sysctlnode *congctl_node;
	const struct sysctlnode *mslt_node;
	const struct sysctlnode *vtw_node;
#ifdef TCP_DEBUG
	extern struct tcp_debug tcp_debug[TCP_NDEBUG];
	extern int tcp_debx;
#endif

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, pfname, NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, tcpname,
		       SYSCTL_DESCR("TCP related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rfc1323",
		       SYSCTL_DESCR("Enable RFC1323 TCP extensions"),
		       sysctl_update_tcpcb_template, 0, &tcp_do_rfc1323, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RFC1323, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sendspace",
		       SYSCTL_DESCR("Default TCP send buffer size"),
		       NULL, 0, &tcp_sendspace, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SENDSPACE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "recvspace",
		       SYSCTL_DESCR("Default TCP receive buffer size"),
		       NULL, 0, &tcp_recvspace, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RECVSPACE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mssdflt",
		       SYSCTL_DESCR("Default maximum segment size"),
		       sysctl_net_inet_tcp_mssdflt, 0, &tcp_mssdflt, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_MSSDFLT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "minmss",
		       SYSCTL_DESCR("Lower limit for TCP maximum segment size"),
		       NULL, 0, &tcp_minmss, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "msl",
		       SYSCTL_DESCR("Maximum Segment Life"),
		       NULL, 0, &tcp_msl, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_MSL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "syn_cache_limit",
		       SYSCTL_DESCR("Maximum number of entries in the TCP "
				    "compressed state engine"),
		       NULL, 0, &tcp_syn_cache_limit, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SYN_CACHE_LIMIT,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "syn_bucket_limit",
		       SYSCTL_DESCR("Maximum number of entries per hash "
				    "bucket in the TCP compressed state "
				    "engine"),
		       NULL, 0, &tcp_syn_bucket_limit, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SYN_BUCKET_LIMIT,
		       CTL_EOL);
#if 0 /* obsoleted */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "syn_cache_interval",
		       SYSCTL_DESCR("TCP compressed state engine's timer interval"),
		       NULL, 0, &tcp_syn_cache_interval, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SYN_CACHE_INTER,
		       CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "init_win",
		       SYSCTL_DESCR("Initial TCP congestion window"),
		       sysctl_tcp_init_win, 0, &tcp_init_win, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_INIT_WIN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mss_ifmtu",
		       SYSCTL_DESCR("Use interface MTU for calculating MSS"),
		       NULL, 0, &tcp_mss_ifmtu, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_MSS_IFMTU, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "sack",
		       SYSCTL_DESCR("RFC2018 Selective ACKnowledgement tunables"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_EOL);

	/* Congctl subtree */
	sysctl_createv(clog, 0, NULL, &congctl_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "congctl",
		       SYSCTL_DESCR("TCP Congestion Control"),
	    	       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &congctl_node, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "available",
		       SYSCTL_DESCR("Available Congestion Control Mechanisms"),
		       NULL, 0, tcp_congctl_avail, 0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &congctl_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRING, "selected",
		       SYSCTL_DESCR("Selected Congestion Control Mechanism"),
		       sysctl_tcp_congctl, 0, NULL, TCPCC_MAXLEN,
		       CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "win_scale",
		       SYSCTL_DESCR("Use RFC1323 window scale options"),
		       sysctl_update_tcpcb_template, 0, &tcp_do_win_scale, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_WSCALE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "timestamps",
		       SYSCTL_DESCR("Use RFC1323 time stamp options"),
		       sysctl_update_tcpcb_template, 0, &tcp_do_timestamps, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_TSTAMP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "compat_42",
		       SYSCTL_DESCR("Enable workarounds for 4.2BSD TCP bugs"),
		       NULL, 0, &tcp_compat_42, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_COMPAT_42, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "cwm",
		       SYSCTL_DESCR("Hughes/Touch/Heidemann Congestion Window "
				    "Monitoring"),
		       NULL, 0, &tcp_cwm, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_CWM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "cwm_burstsize",
		       SYSCTL_DESCR("Congestion Window Monitoring allowed "
				    "burst count in packets"),
		       NULL, 0, &tcp_cwm_burstsize, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_CWM_BURSTSIZE,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "ack_on_push",
		       SYSCTL_DESCR("Immediately return ACK when PSH is "
				    "received"),
		       NULL, 0, &tcp_ack_on_push, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_ACK_ON_PUSH, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepidle",
		       SYSCTL_DESCR("Allowed connection idle ticks before a "
				    "keepalive probe is sent"),
		       sysctl_tcp_keep, 0, &tcp_keepidle, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_KEEPIDLE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepintvl",
		       SYSCTL_DESCR("Ticks before next keepalive probe is sent"),
		       sysctl_tcp_keep, 0, &tcp_keepintvl, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_KEEPINTVL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepcnt",
		       SYSCTL_DESCR("Number of keepalive probes to send"),
		       sysctl_tcp_keep, 0, &tcp_keepcnt, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_KEEPCNT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "slowhz",
		       SYSCTL_DESCR("Keepalive ticks per second"),
		       NULL, PR_SLOWHZ, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SLOWHZ, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "log_refused",
		       SYSCTL_DESCR("Log refused TCP connections"),
		       NULL, 0, &tcp_log_refused, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_LOG_REFUSED, CTL_EOL);
#if 0 /* obsoleted */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rstratelimit", NULL,
		       NULL, 0, &tcp_rst_ratelim, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RSTRATELIMIT, CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rstppslimit",
		       SYSCTL_DESCR("Maximum number of RST packets to send "
				    "per second"),
		       NULL, 0, &tcp_rst_ppslim, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RSTPPSLIMIT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "delack_ticks",
		       SYSCTL_DESCR("Number of ticks to delay sending an ACK"),
		       NULL, 0, &tcp_delack_ticks, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_DELACK_TICKS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "init_win_local",
		       SYSCTL_DESCR("Initial TCP window size (in segments)"),
		       sysctl_tcp_init_win, 0, &tcp_init_win_local, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_INIT_WIN_LOCAL,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "ident",
		       SYSCTL_DESCR("RFC1413 Identification Protocol lookups"),
		       sysctl_net_inet_tcp_ident, 0, NULL, sizeof(uid_t),
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_IDENT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "do_loopback_cksum",
		       SYSCTL_DESCR("Perform TCP checksum on loopback"),
		       NULL, 0, &tcp_do_loopback_cksum, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_LOOPBACKCKSUM,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("TCP protocol control block list"),
		       sysctl_inpcblist, 0, &tcbtable, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepinit",
		       SYSCTL_DESCR("Ticks before initial tcp connection times out"),
		       sysctl_tcp_keep, 0, &tcp_keepinit, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);

	/* TCP socket buffers auto-sizing nodes */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "recvbuf_auto",
		       SYSCTL_DESCR("Enable automatic receive "
		           "buffer sizing (experimental)"),
		       NULL, 0, &tcp_do_autorcvbuf, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "recvbuf_inc",
		       SYSCTL_DESCR("Incrementor step size of "
		           "automatic receive buffer"),
		       NULL, 0, &tcp_autorcvbuf_inc, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "recvbuf_max",
		       SYSCTL_DESCR("Max size of automatic receive buffer"),
		       NULL, 0, &tcp_autorcvbuf_max, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sendbuf_auto",
		       SYSCTL_DESCR("Enable automatic send "
		           "buffer sizing (experimental)"),
		       NULL, 0, &tcp_do_autosndbuf, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sendbuf_inc",
		       SYSCTL_DESCR("Incrementor step size of "
		           "automatic send buffer"),
		       NULL, 0, &tcp_autosndbuf_inc, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sendbuf_max",
		       SYSCTL_DESCR("Max size of automatic send buffer"),
		       NULL, 0, &tcp_autosndbuf_max, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);

	/* ECN subtree */
	sysctl_createv(clog, 0, NULL, &ecn_node,
	    	       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ecn",
	    	       SYSCTL_DESCR("RFC3168 Explicit Congestion Notification"),
	    	       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &ecn_node, NULL,
	    	       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enable",
		       SYSCTL_DESCR("Enable TCP Explicit Congestion "
			   "Notification"),
	    	       NULL, 0, &tcp_do_ecn, 0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &ecn_node, NULL,
	    	       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxretries",
		       SYSCTL_DESCR("Number of times to retry ECN setup "
			       "before disabling ECN on the connection"),
	    	       NULL, 0, &tcp_ecn_maxretries, 0, CTL_CREATE, CTL_EOL);
	
	/* SACK gets its own little subtree. */
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enable",
		       SYSCTL_DESCR("Enable RFC2018 Selective ACKnowledgement"),
		       NULL, 0, &tcp_do_sack, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxholes",
		       SYSCTL_DESCR("Maximum number of TCP SACK holes allowed per connection"),
		       NULL, 0, &tcp_sack_tp_maxholes, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "globalmaxholes",
		       SYSCTL_DESCR("Global maximum number of TCP SACK holes"),
		       NULL, 0, &tcp_sack_globalmaxholes, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "globalholes",
		       SYSCTL_DESCR("Global number of TCP SACK holes"),
		       NULL, 0, &tcp_sack_globalholes, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "stats",
		       SYSCTL_DESCR("TCP statistics"),
		       sysctl_net_inet_tcp_stats, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_STATS,
		       CTL_EOL);
        sysctl_createv(clog, 0, NULL, NULL,
                       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
                       CTLTYPE_INT, "local_by_rtt",
                       SYSCTL_DESCR("Use RTT estimator to decide which hosts "
				    "are local"),
		       NULL, 0, &tcp_rttlocal, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
#ifdef TCP_DEBUG
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "debug",
		       SYSCTL_DESCR("TCP sockets debug information"),
		       NULL, 0, &tcp_debug, sizeof(tcp_debug),
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_DEBUG,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "debx",
		       SYSCTL_DESCR("Number of TCP debug sockets messages"),
		       NULL, 0, &tcp_debx, sizeof(tcp_debx),
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_DEBX,
		       CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "drop",
		       SYSCTL_DESCR("TCP drop connection"),
		       sysctl_net_inet_tcp_drop, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_DROP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "iss_hash",
		       SYSCTL_DESCR("Enable RFC 1948 ISS by cryptographic "
				    "hash computation"),
		       NULL, 0, &tcp_do_rfc1948, sizeof(tcp_do_rfc1948),
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE,
		       CTL_EOL);

	/* ABC subtree */

	sysctl_createv(clog, 0, NULL, &abc_node,
		       CTLFLAG_PERMANENT, CTLTYPE_NODE, "abc",
		       SYSCTL_DESCR("RFC3465 Appropriate Byte Counting (ABC)"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &abc_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enable",
		       SYSCTL_DESCR("Enable RFC3465 Appropriate Byte Counting"),
		       NULL, 0, &tcp_do_abc, 0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &abc_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "aggressive",
		       SYSCTL_DESCR("1: L=2*SMSS 0: L=1*SMSS"),
		       NULL, 0, &tcp_abc_aggressive, 0, CTL_CREATE, CTL_EOL);

	/* MSL tuning subtree */

	sysctl_createv(clog, 0, NULL, &mslt_node,
		       CTLFLAG_PERMANENT, CTLTYPE_NODE, "mslt",
		       SYSCTL_DESCR("MSL Tuning for TIME_WAIT truncation"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &mslt_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enable",
		       SYSCTL_DESCR("Enable TIME_WAIT truncation"),
		       NULL, 0, &tcp_msl_enable, 0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &mslt_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "loopback",
		       SYSCTL_DESCR("MSL value to use for loopback connections"),
		       NULL, 0, &tcp_msl_loop, 0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &mslt_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "local",
		       SYSCTL_DESCR("MSL value to use for local connections"),
		       NULL, 0, &tcp_msl_local, 0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &mslt_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "remote",
		       SYSCTL_DESCR("MSL value to use for remote connections"),
		       NULL, 0, &tcp_msl_remote, 0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &mslt_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "remote_threshold",
		       SYSCTL_DESCR("RTT estimate value to promote local to remote"), 
		       NULL, 0, &tcp_msl_remote_threshold, 0, CTL_CREATE, CTL_EOL);

	/* vestigial TIME_WAIT tuning subtree */

	sysctl_createv(clog, 0, NULL, &vtw_node,
		       CTLFLAG_PERMANENT, CTLTYPE_NODE, "vtw",
		       SYSCTL_DESCR("Tuning for Vestigial TIME_WAIT"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &vtw_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enable",
		       SYSCTL_DESCR("Enable Vestigial TIME_WAIT"),
		       sysctl_tcp_vtw_enable, 0,
	               (pf == AF_INET) ? &tcp4_vtw_enable : &tcp6_vtw_enable,
		       0, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &vtw_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
		       CTLTYPE_INT, "entries",
		       SYSCTL_DESCR("Maximum number of vestigial TIME_WAIT entries"),
		       NULL, 0, &tcp_vtw_entries, 0, CTL_CREATE, CTL_EOL);
}

void
tcp_usrreq_init(void)
{

#ifdef INET
	sysctl_net_inet_tcp_setup2(NULL, PF_INET, "inet", "tcp");
#endif
#ifdef INET6
	sysctl_net_inet_tcp_setup2(NULL, PF_INET6, "inet6", "tcp6");
#endif
}

PR_WRAP_USRREQS(tcp)
#define	tcp_attach	tcp_attach_wrapper
#define	tcp_detach	tcp_detach_wrapper
#define	tcp_accept	tcp_accept_wrapper
#define	tcp_bind	tcp_bind_wrapper
#define	tcp_listen	tcp_listen_wrapper
#define	tcp_connect	tcp_connect_wrapper
#define	tcp_connect2	tcp_connect2_wrapper
#define	tcp_disconnect	tcp_disconnect_wrapper
#define	tcp_shutdown	tcp_shutdown_wrapper
#define	tcp_abort	tcp_abort_wrapper
#define	tcp_ioctl	tcp_ioctl_wrapper
#define	tcp_stat	tcp_stat_wrapper
#define	tcp_peeraddr	tcp_peeraddr_wrapper
#define	tcp_sockaddr	tcp_sockaddr_wrapper
#define	tcp_rcvd	tcp_rcvd_wrapper
#define	tcp_recvoob	tcp_recvoob_wrapper
#define	tcp_send	tcp_send_wrapper
#define	tcp_sendoob	tcp_sendoob_wrapper
#define	tcp_purgeif	tcp_purgeif_wrapper

const struct pr_usrreqs tcp_usrreqs = {
	.pr_attach	= tcp_attach,
	.pr_detach	= tcp_detach,
	.pr_accept	= tcp_accept,
	.pr_bind	= tcp_bind,
	.pr_listen	= tcp_listen,
	.pr_connect	= tcp_connect,
	.pr_connect2	= tcp_connect2,
	.pr_disconnect	= tcp_disconnect,
	.pr_shutdown	= tcp_shutdown,
	.pr_abort	= tcp_abort,
	.pr_ioctl	= tcp_ioctl,
	.pr_stat	= tcp_stat,
	.pr_peeraddr	= tcp_peeraddr,
	.pr_sockaddr	= tcp_sockaddr,
	.pr_rcvd	= tcp_rcvd,
	.pr_recvoob	= tcp_recvoob,
	.pr_send	= tcp_send,
	.pr_sendoob	= tcp_sendoob,
	.pr_purgeif	= tcp_purgeif,
};
