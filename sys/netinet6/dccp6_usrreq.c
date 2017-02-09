/*	$KAME: dccp6_usrreq.c,v 1.13 2005/07/27 08:42:56 nishida Exp $	*/
/*	$NetBSD: dccp6_usrreq.c,v 1.7 2015/08/24 22:21:27 pooka Exp $ */

/*
 * Copyright (C) 2003 WIDE Project.
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
__KERNEL_RCSID(0, "$NetBSD: dccp6_usrreq.c,v 1.7 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_dccp.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/dccp.h>
#include <netinet/dccp_var.h>
#include <netinet6/dccp6_var.h>
#include <netinet/dccp_cc_sw.h>

#if !defined(__FreeBSD__) || __FreeBSD_version < 500000
#define	INP_INFO_LOCK_INIT(x,y)
#define	INP_INFO_WLOCK(x)
#define INP_INFO_WUNLOCK(x)
#define	INP_INFO_RLOCK(x)
#define INP_INFO_RUNLOCK(x)
#define	INP_LOCK(x)
#define INP_UNLOCK(x)
#endif

#define PULLDOWN_TEST

int
dccp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	DCCP_DEBUG((LOG_INFO, "In dccp6_input!\n"));
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, *offp, sizeof(struct dccphdr), IPPROTO_DONE);
#endif

	dccp_input(m, *offp);
	return IPPROTO_DONE;
}

void *
dccp6_ctlinput(int cmd, const struct sockaddr *sa, void *d)
{
	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return NULL;
	
	/* FIX LATER */
	return NULL;
}

int
dccp6_bind(struct socket *so, struct sockaddr *nam, struct lwp *td)
{
	struct in6pcb *in6p;
	int error;
	struct sockaddr_in6 *sin6p = (struct sockaddr_in6 *)nam;

	DCCP_DEBUG((LOG_INFO, "Entering dccp6_bind!\n"));
	INP_INFO_WLOCK(&dccpbinfo);
	in6p = sotoin6pcb(so);
	if (in6p == 0) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		DCCP_DEBUG((LOG_INFO, "dccp6_bind: in6p == 0!\n"));
		return EINVAL;
	}
	/* Do not bind to multicast addresses! */
	if (sin6p->sin6_family == AF_INET6 &&
	    IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr)) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EAFNOSUPPORT;
	}
	INP_LOCK(inp);

	in6todccpcb(in6p)->inp_vflag &= ~INP_IPV4;
	in6todccpcb(in6p)->inp_vflag |= INP_IPV6;
	
	error = in6_pcbbind(in6p, sin6p, td);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&dccpbinfo);
	return error;
}

int
dccp6_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct in6pcb *in6p;
	struct dccpcb *dp;
	int error;
	struct sockaddr_in6 *sin6;
	char test[2];

	DCCP_DEBUG((LOG_INFO, "Entering dccp6_connect!\n"));

#ifndef __NetBSD__
	INP_INFO_WLOCK(&dccpbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		INP_UNLOCK(inp);
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EISCONN;
	}

	dp = (struct dccpcb *)inp->inp_ppcb;
#else
	in6p = sotoin6pcb(so);
	if (in6p == 0) {
		return EINVAL;
	}
	if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
		return EISCONN;
	}

	dp = (struct dccpcb *)in6p->in6p_ppcb;
#endif
	if (dp->state == DCCPS_ESTAB) {
		DCCP_DEBUG((LOG_INFO, "Why are we in connect when we already have a established connection?\n"));
	}

	dp->who = DCCP_CLIENT;
	dp->seq_snd = (((u_int64_t)random() << 32) | random()) % 281474976710656LL;

	sin6 = (struct sockaddr_in6 *)nam;
	if (sin6->sin6_family == AF_INET6
	    && IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
		error = EAFNOSUPPORT;
		goto bad;
	}

	dp->inp_vflag &= ~INP_IPV4;
	dp->inp_vflag |= INP_IPV6;

	error = dccp_doconnect(so, nam, l, 1);

	if (error != 0)
		goto bad;

	callout_reset(&dp->retrans_timer, dp->retrans, dccp_retrans_t, dp);
	callout_reset(&dp->connect_timer, DCCP_CONNECT_TIMER, dccp_connect_t, dp);

	test[0] = dp->pref_cc;
#if 0
	/* FIX THIS LATER */
	if (dp->pref_cc == 2) {
		test[1] = 3;
	} else {
		test[1] = 2;
	}
	dccp_add_feature(dp, DCCP_OPT_CHANGE, DCCP_FEATURE_CC, test, 2);
	dccp_add_feature(dp, DCCP_OPT_PREFER, DCCP_FEATURE_CC, test, 2);
#else
	 /* we only support CCID2 at this moment */
	dccp_add_feature(dp, DCCP_OPT_CHANGE_R, DCCP_FEATURE_CC, test, 1);
#endif

	error = dccp_output(dp, 0);

bad:
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&dccpbinfo);
	return error;
}


int
dccp6_listen(struct socket *so, struct lwp *l)
{
	struct in6pcb *in6p;
	struct dccpcb *dp;
	int error = 0;

	DCCP_DEBUG((LOG_INFO, "Entering dccp6_listen!\n"));

	INP_INFO_RLOCK(&dccpbinfo);
	in6p = sotoin6pcb(so);
	if (in6p == 0) {
		INP_INFO_RUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&dccpbinfo);
	dp = in6todccpcb(in6p);
	DCCP_DEBUG((LOG_INFO, "Checking in6p->in6p_lport!\n"));
	if (in6p->in6p_lport == 0) {
		error = in6_pcbbind(in6p, NULL, l);
	}
	if (error == 0) {
		dp->state = DCCPS_LISTEN;
		dp->who = DCCP_LISTENER;
		dp->seq_snd = 512;
	}
	INP_UNLOCK(inp);
	return error;
}

int
dccp6_accept(struct socket *so, struct sockaddr *nam)
{
	struct in6pcb *in6p = NULL;
	int error = 0;

	DCCP_DEBUG((LOG_INFO, "Entering dccp6_accept!\n"));
	if (nam == NULL) {
		return EINVAL;
	}
	if (so->so_state & SS_ISDISCONNECTED) {
		DCCP_DEBUG((LOG_INFO, "so_state && SS_ISDISCONNECTED!, so->state = %i\n", so->so_state));
		return ECONNABORTED;
	}

	INP_INFO_RLOCK(&dccpbinfo);
	in6p = sotoin6pcb(so);
	if (in6p == 0) {
		INP_INFO_RUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&dccpbinfo);
	in6_setpeeraddr(in6p, (struct sockaddr_in6 *)nam);

	INP_UNLOCK(inp);
	return error;
}

static int
dccp6_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	int error = 0;
	int family;

	family = so->so_proto->pr_domain->dom_family;
	switch (family) {
	case PF_INET6:
		error = in6_control(so, cmd, nam, ifp);
		break;
	default:
		error =	 EAFNOSUPPORT;
	}
	return (error);
}

static int
dccp6_stat(struct socket *so, struct stat *ub)
{
	return 0;
}

static int
dccp6_purgeif(struct socket *so, struct ifnet *ifp)
{
	int s;

	s = splsoftnet();
	mutex_enter(softnet_lock);
	in6_pcbpurgeif0(&dccpbtable, ifp);
	in6_purgeif(ifp);
	in6_pcbpurgeif(&dccpbtable, ifp);
	mutex_exit(softnet_lock);
	splx(s);

	return 0;
}

static int
dccp6_attach(struct socket *so, int proto)
{
	return dccp_attach(so, proto);
}

static int
dccp6_detach(struct socket *so)
{
	return dccp_detach(so);
}

static int
dccp6_connect2(struct socket *so, struct socket *so2)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
dccp6_disconnect(struct socket *so)
{
	return dccp_disconnect(so);
}

static int
dccp6_shutdown(struct socket *so)
{
	return dccp_shutdown(so);
}

static int
dccp6_abort(struct socket *so)
{
	return dccp_abort(so);
}


static int
dccp6_peeraddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));
	KASSERT(sotoinpcb(so) != NULL);
	KASSERT(nam != NULL);

	in6_setpeeraddr(sotoin6pcb(so), (struct sockaddr_in6 *)nam);
	return 0;
}

static int
dccp6_sockaddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));
	KASSERT(sotoinpcb(so) != NULL);
	KASSERT(nam != NULL);

	in6_setsockaddr(sotoin6pcb(so), (struct sockaddr_in6 *)nam);
	return 0;
}

static int
dccp6_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
dccp6_rcvd(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
dccp6_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	return dccp_send(so, m, nam, control, l);
}

static int
dccp6_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	m_freem(m);
	m_freem(control);

	return EOPNOTSUPP;
}


PR_WRAP_USRREQS(dccp6)
#define	dccp6_attach		dccp6_attach_wrapper
#define	dccp6_detach		dccp6_detach_wrapper
#define dccp6_accept		dccp6_accept_wrapper
#define	dccp6_bind		dccp6_bind_wrapper
#define	dccp6_listen		dccp6_listen_wrapper
#define	dccp6_connect		dccp6_connect_wrapper
#define	dccp6_connect2		dccp6_connect2_wrapper
#define	dccp6_disconnect	dccp6_disconnect_wrapper
#define	dccp6_shutdown		dccp6_shutdown_wrapper
#define	dccp6_abort		dccp6_abort_wrapper
#define	dccp6_ioctl		dccp6_ioctl_wrapper
#define	dccp6_stat		dccp6_stat_wrapper
#define	dccp6_peeraddr		dccp6_peeraddr_wrapper
#define	dccp6_sockaddr		dccp6_sockaddr_wrapper
#define	dccp6_rcvd		dccp6_rcvd_wrapper
#define	dccp6_recvoob		dccp6_recvoob_wrapper
#define	dccp6_send		dccp6_send_wrapper
#define	dccp6_sendoob		dccp6_sendoob_wrapper
#define	dccp6_purgeif		dccp6_purgeif_wrapper

const struct pr_usrreqs dccp6_usrreqs = {
	.pr_attach	= dccp6_attach,
	.pr_detach	= dccp6_detach,
	.pr_accept	= dccp6_accept,
	.pr_bind	= dccp6_bind,
	.pr_listen	= dccp6_listen,
	.pr_connect	= dccp6_connect,
	.pr_connect2	= dccp6_connect2,
	.pr_disconnect	= dccp6_disconnect,
	.pr_shutdown	= dccp6_shutdown,
	.pr_abort	= dccp6_abort,
	.pr_ioctl	= dccp6_ioctl,
	.pr_stat	= dccp6_stat,
	.pr_peeraddr	= dccp6_peeraddr,
	.pr_sockaddr	= dccp6_sockaddr,
	.pr_rcvd	= dccp6_rcvd,
	.pr_recvoob	= dccp6_recvoob,
	.pr_send	= dccp6_send,
	.pr_sendoob	= dccp6_sendoob,
	.pr_purgeif	= dccp6_purgeif,
};
