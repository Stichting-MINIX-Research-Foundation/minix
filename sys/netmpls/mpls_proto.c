/*	$NetBSD: mpls_proto.c,v 1.29 2015/08/24 22:21:27 pooka Exp $ */

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mihai Chelaru <kefren@NetBSD.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpls_proto.c,v 1.29 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_mbuftrace.h"
#endif

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/route.h>

#include <netmpls/mpls.h>
#include <netmpls/mpls_var.h>

struct ifqueue mplsintrq;

static int mpls_attach(struct socket *, int);
static void sysctl_net_mpls_setup(struct sysctllog **);

#ifdef MBUFTRACE
struct mowner mpls_owner = MOWNER_INIT("MPLS", "");
#endif

int mpls_defttl = 255;
int mpls_mapttl_inet = 1;
int mpls_mapttl_inet6 = 1;
int mpls_icmp_respond = 0;
int mpls_forwarding = 0;
int mpls_frame_accept = 0;
int mpls_mapprec_inet = 1;
int mpls_mapclass_inet6 = 1;
int mpls_rfc4182 = 1;

void mpls_init(void)
{
#ifdef MBUFTRACE
	MOWNER_ATTACH(&mpls_owner);
#endif
	memset(&mplsintrq, 0, sizeof(mplsintrq));
	mplsintrq.ifq_maxlen = 256;

	sysctl_net_mpls_setup(NULL);
}

static int
mpls_attach(struct socket *so, int proto)
{
	int error = EOPNOTSUPP;

	sosetlock(so);
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, 8192, 8192);
	}
	return error;
}

static void
mpls_detach(struct socket *so)
{
}

static int
mpls_accept(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_listen(struct socket *so, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_connect2(struct socket *so, struct socket *so2)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_disconnect(struct socket *so)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_shutdown(struct socket *so)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_abort(struct socket *so)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	return EOPNOTSUPP;
}

static int
mpls_stat(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_peeraddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_sockaddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_rcvd(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
mpls_purgeif(struct socket *so, struct ifnet *ifp)
{

	return EOPNOTSUPP;
}

/*
 * Sysctl for MPLS variables.
 */
static void
sysctl_net_mpls_setup(struct sysctllog **clog)
{

        sysctl_createv(clog, 0, NULL, NULL,
                       CTLFLAG_PERMANENT,
                       CTLTYPE_NODE, "mpls", NULL,
                       NULL, 0, NULL, 0,
                       CTL_NET, PF_MPLS, CTL_EOL);

        sysctl_createv(clog, 0, NULL, NULL,
                       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
                       CTLTYPE_INT, "ttl",
                       SYSCTL_DESCR("Default TTL"),
                       NULL, 0, &mpls_defttl, 0,
                       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "forwarding",
		       SYSCTL_DESCR("MPLS forwarding"),
		       NULL, 0, &mpls_forwarding, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "accept",
		       SYSCTL_DESCR("Accept MPLS Frames"),
		       NULL, 0, &mpls_frame_accept, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "ifq_len",
		       SYSCTL_DESCR("MPLS queue length"),
		       NULL, 0, &mplsintrq.ifq_maxlen, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rfc4182",
		       SYSCTL_DESCR("RFC 4182 conformance"),
		       NULL, 0, &mpls_rfc4182, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
#ifdef INET
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "inet_mapttl",
		       SYSCTL_DESCR("Map IP TTL"),
		       NULL, 0, &mpls_mapttl_inet, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "inet_map_prec",
		       SYSCTL_DESCR("Map IP Prec"),
		       NULL, 0, &mpls_mapprec_inet, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "icmp_respond",
		       SYSCTL_DESCR("Emit ICMP packets on errors"),
		       NULL, 0, &mpls_icmp_respond, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
#endif
#ifdef INET6
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "inet6_mapttl",
		       SYSCTL_DESCR("Map IP6 TTL"),
		       NULL, 0, &mpls_mapttl_inet6, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "inet6_map_prec",
		       SYSCTL_DESCR("Map IP6 class"),
		       NULL, 0, &mpls_mapclass_inet6, 0,
		       CTL_NET, PF_MPLS, CTL_CREATE, CTL_EOL);
#endif
}

DOMAIN_DEFINE(mplsdomain);

PR_WRAP_USRREQS(mpls)
#define	mpls_attach	mpls_attach_wrapper
#define	mpls_detach	mpls_detach_wrapper
#define	mpls_accept	mpls_accept_wrapper
#define	mpls_bind	mpls_bind_wrapper
#define	mpls_listen	mpls_listen_wrapper
#define	mpls_connect	mpls_connect_wrapper
#define	mpls_connect2	mpls_connect2_wrapper
#define	mpls_disconnect	mpls_disconnect_wrapper
#define	mpls_shutdown	mpls_shutdown_wrapper
#define	mpls_abort	mpls_abort_wrapper
#define	mpls_ioctl	mpls_ioctl_wrapper
#define	mpls_stat	mpls_stat_wrapper
#define	mpls_peeraddr	mpls_peeraddr_wrapper
#define	mpls_sockaddr	mpls_sockaddr_wrapper
#define	mpls_rcvd	mpls_rcvd_wrapper
#define	mpls_recvoob	mpls_recvoob_wrapper
#define	mpls_send	mpls_send_wrapper
#define	mpls_sendoob	mpls_sendoob_wrapper
#define	mpls_purgeif	mpls_purgeif_wrapper

static const struct pr_usrreqs mpls_usrreqs = {
	.pr_attach	= mpls_attach,
	.pr_detach	= mpls_detach,
	.pr_accept	= mpls_accept,
	.pr_bind	= mpls_bind,
	.pr_listen	= mpls_listen,
	.pr_connect	= mpls_connect,
	.pr_connect2	= mpls_connect2,
	.pr_disconnect	= mpls_disconnect,
	.pr_shutdown	= mpls_shutdown,
	.pr_abort	= mpls_abort,
	.pr_ioctl	= mpls_ioctl,
	.pr_stat	= mpls_stat,
	.pr_peeraddr	= mpls_peeraddr,
	.pr_sockaddr	= mpls_sockaddr,
	.pr_rcvd	= mpls_rcvd,
	.pr_recvoob	= mpls_recvoob,
	.pr_send	= mpls_send,
	.pr_sendoob	= mpls_sendoob,
	.pr_purgeif	= mpls_purgeif,
};

const struct protosw mplssw[] = {
	{	.pr_domain = &mplsdomain,
		.pr_init = mpls_init,
	},
	{
		.pr_type = SOCK_DGRAM,
		.pr_domain = &mplsdomain,
		.pr_flags = PR_ATOMIC | PR_ADDR,
		.pr_usrreqs = &mpls_usrreqs,
	},
	{
		.pr_type = SOCK_RAW,
		.pr_domain = &mplsdomain,
		.pr_flags = PR_ATOMIC | PR_ADDR,
		.pr_usrreqs = &mpls_usrreqs,
	},
};

struct domain mplsdomain = {
	.dom_family = PF_MPLS,
	.dom_name = "MPLS",
	.dom_init = NULL,
	.dom_externalize = NULL,
	.dom_dispose = NULL, 
	.dom_protosw = mplssw,
	.dom_protoswNPROTOSW = &mplssw[__arraycount(mplssw)],
	.dom_rtattach = rt_inithead,
	.dom_rtoffset = offsetof(struct sockaddr_mpls, smpls_addr) << 3,
	.dom_maxrtkey = sizeof(union mpls_shim),
	.dom_ifattach = NULL,
	.dom_ifdetach = NULL,
	.dom_ifqueues = { &mplsintrq, NULL },
	.dom_link = { NULL },
	.dom_mowner = MOWNER_INIT("MPLS", ""),
	.dom_sa_cmpofs = offsetof(struct sockaddr_mpls, smpls_addr),
	.dom_sa_cmplen = sizeof(union mpls_shim),
	.dom_rtcache = LIST_HEAD_INITIALIZER(mplsdomain.dom_rtcache)
};
