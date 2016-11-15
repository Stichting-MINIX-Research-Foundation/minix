/*	$NetBSD: ddp_usrreq.c,v 1.68 2015/05/02 17:18:03 rtr Exp $	 */

/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ddp_usrreq.c,v 1.68 2015/05/02 17:18:03 rtr Exp $");

#include "opt_mbuftrace.h"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_ether.h>
#include <net/net_stats.h>
#include <netinet/in.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp_var.h>
#include <netatalk/ddp_private.h>
#include <netatalk/aarp.h>
#include <netatalk/at_extern.h>

static void at_pcbdisconnect(struct ddpcb *);
static void at_sockaddr(struct ddpcb *, struct sockaddr_at *);
static int at_pcbsetaddr(struct ddpcb *, struct sockaddr_at *);
static int at_pcbconnect(struct ddpcb *, struct sockaddr_at *);
static void ddp_detach(struct socket *);

struct ifqueue atintrq1, atintrq2;
struct ddpcb   *ddp_ports[ATPORT_LAST];
struct ddpcb   *ddpcb = NULL;
percpu_t *ddpstat_percpu;
struct at_ifaddrhead at_ifaddr;		/* Here as inited in this file */
u_long ddp_sendspace = DDP_MAXSZ;	/* Max ddp size + 1 (ddp_type) */
u_long ddp_recvspace = 25 * (587 + sizeof(struct sockaddr_at));

#ifdef MBUFTRACE
struct mowner atalk_rx_mowner = MOWNER_INIT("atalk", "rx");
struct mowner atalk_tx_mowner = MOWNER_INIT("atalk", "tx");
#endif

static void
at_sockaddr(struct ddpcb *ddp, struct sockaddr_at *addr)
{

	*addr = ddp->ddp_lsat;
}

static int
at_pcbsetaddr(struct ddpcb *ddp, struct sockaddr_at *sat)
{
	struct sockaddr_at lsat;
	struct at_ifaddr *aa;
	struct ddpcb   *ddpp;

	if (ddp->ddp_lsat.sat_port != ATADDR_ANYPORT) {	/* shouldn't be bound */
		return (EINVAL);
	}
	if (NULL != sat) {	/* validate passed address */

		if (sat->sat_family != AF_APPLETALK)
			return (EAFNOSUPPORT);

		if (sat->sat_addr.s_node != ATADDR_ANYNODE ||
		    sat->sat_addr.s_net != ATADDR_ANYNET) {
			TAILQ_FOREACH(aa, &at_ifaddr, aa_list) {
				if ((sat->sat_addr.s_net ==
				    AA_SAT(aa)->sat_addr.s_net) &&
				    (sat->sat_addr.s_node ==
				    AA_SAT(aa)->sat_addr.s_node))
					break;
			}
			if (!aa)
				return (EADDRNOTAVAIL);
		}
		if (sat->sat_port != ATADDR_ANYPORT) {
			int error;

			if (sat->sat_port < ATPORT_FIRST ||
			    sat->sat_port >= ATPORT_LAST)
				return (EINVAL);

			if (sat->sat_port < ATPORT_RESERVED &&
			    (error = kauth_authorize_network(curlwp->l_cred,
			    KAUTH_NETWORK_BIND, KAUTH_REQ_NETWORK_BIND_PRIVPORT,
			    ddpcb->ddp_socket, sat, NULL)) != 0)
				return (error);
		}
	} else {
		memset((void *) & lsat, 0, sizeof(struct sockaddr_at));
		lsat.sat_len = sizeof(struct sockaddr_at);
		lsat.sat_addr.s_node = ATADDR_ANYNODE;
		lsat.sat_addr.s_net = ATADDR_ANYNET;
		lsat.sat_family = AF_APPLETALK;
		sat = &lsat;
	}

	if (sat->sat_addr.s_node == ATADDR_ANYNODE &&
	    sat->sat_addr.s_net == ATADDR_ANYNET) {
		if (TAILQ_EMPTY(&at_ifaddr))
			return EADDRNOTAVAIL;
		sat->sat_addr = AA_SAT(TAILQ_FIRST(&at_ifaddr))->sat_addr;
	}
	ddp->ddp_lsat = *sat;

	/*
         * Choose port.
         */
	if (sat->sat_port == ATADDR_ANYPORT) {
		for (sat->sat_port = ATPORT_RESERVED;
		     sat->sat_port < ATPORT_LAST; sat->sat_port++) {
			if (ddp_ports[sat->sat_port - 1] == 0)
				break;
		}
		if (sat->sat_port == ATPORT_LAST) {
			return (EADDRNOTAVAIL);
		}
		ddp->ddp_lsat.sat_port = sat->sat_port;
		ddp_ports[sat->sat_port - 1] = ddp;
	} else {
		for (ddpp = ddp_ports[sat->sat_port - 1]; ddpp;
		     ddpp = ddpp->ddp_pnext) {
			if (ddpp->ddp_lsat.sat_addr.s_net ==
			    sat->sat_addr.s_net &&
			    ddpp->ddp_lsat.sat_addr.s_node ==
			    sat->sat_addr.s_node)
				break;
		}
		if (ddpp != NULL)
			return (EADDRINUSE);

		ddp->ddp_pnext = ddp_ports[sat->sat_port - 1];
		ddp_ports[sat->sat_port - 1] = ddp;
		if (ddp->ddp_pnext)
			ddp->ddp_pnext->ddp_pprev = ddp;
	}

	return 0;
}

static int
at_pcbconnect(struct ddpcb *ddp, struct sockaddr_at *sat)
{
	struct rtentry *rt;
	const struct sockaddr_at *cdst;
	struct route *ro;
	struct at_ifaddr *aa;
	struct ifnet   *ifp;
	u_short         hintnet = 0, net;

	if (sat->sat_family != AF_APPLETALK) {
		return EAFNOSUPPORT;
	}
	/*
         * Under phase 2, network 0 means "the network".  We take "the
         * network" to mean the network the control block is bound to.
         * If the control block is not bound, there is an error.
         */
	if (sat->sat_addr.s_net == ATADDR_ANYNET
	    && sat->sat_addr.s_node != ATADDR_ANYNODE) {
		if (ddp->ddp_lsat.sat_port == ATADDR_ANYPORT) {
			return EADDRNOTAVAIL;
		}
		hintnet = ddp->ddp_lsat.sat_addr.s_net;
	}
	ro = &ddp->ddp_route;
	/*
         * If we've got an old route for this pcb, check that it is valid.
         * If we've changed our address, we may have an old "good looking"
         * route here.  Attempt to detect it.
         */
	if ((rt = rtcache_validate(ro)) != NULL ||
	    (rt = rtcache_update(ro, 1)) != NULL) {
		if (hintnet) {
			net = hintnet;
		} else {
			net = sat->sat_addr.s_net;
		}
		if ((ifp = rt->rt_ifp) != NULL) {
			TAILQ_FOREACH(aa, &at_ifaddr, aa_list) {
				if (aa->aa_ifp == ifp &&
				    ntohs(net) >= ntohs(aa->aa_firstnet) &&
				    ntohs(net) <= ntohs(aa->aa_lastnet)) {
					break;
				}
			}
		} else
			aa = NULL;
		cdst = satocsat(rtcache_getdst(ro));
		if (aa == NULL || (cdst->sat_addr.s_net !=
		    (hintnet ? hintnet : sat->sat_addr.s_net) ||
		    cdst->sat_addr.s_node != sat->sat_addr.s_node)) {
			rtcache_free(ro);
			rt = NULL;
		}
	}
	/*
         * If we've got no route for this interface, try to find one.
         */
	if (rt == NULL) {
		union {
			struct sockaddr		dst;
			struct sockaddr_at	dsta;
		} u;

		sockaddr_at_init(&u.dsta, &sat->sat_addr, 0);
		if (hintnet)
			u.dsta.sat_addr.s_net = hintnet;
		rt = rtcache_lookup(ro, &u.dst);
	}
	/*
         * Make sure any route that we have has a valid interface.
         */
	if (rt != NULL && (ifp = rt->rt_ifp) != NULL) {
		TAILQ_FOREACH(aa, &at_ifaddr, aa_list) {
			if (aa->aa_ifp == ifp)
				break;
		}
	} else
		aa = NULL;
	if (aa == NULL)
		return ENETUNREACH;
	ddp->ddp_fsat = *sat;
	if (ddp->ddp_lsat.sat_port == ATADDR_ANYPORT)
		return at_pcbsetaddr(ddp, NULL);
	return 0;
}

static void
at_pcbdisconnect(struct ddpcb *ddp)
{
	ddp->ddp_fsat.sat_addr.s_net = ATADDR_ANYNET;
	ddp->ddp_fsat.sat_addr.s_node = ATADDR_ANYNODE;
	ddp->ddp_fsat.sat_port = ATADDR_ANYPORT;
}

static int
ddp_attach(struct socket *so, int proto)
{
	struct ddpcb *ddp;
	int error;

	KASSERT(sotoddpcb(so) == NULL);
	sosetlock(so);
#ifdef MBUFTRACE
	so->so_rcv.sb_mowner = &atalk_rx_mowner;
	so->so_snd.sb_mowner = &atalk_tx_mowner;
#endif
	error = soreserve(so, ddp_sendspace, ddp_recvspace);
	if (error) {
		return error;
	}

	ddp = kmem_zalloc(sizeof(*ddp), KM_SLEEP);
	ddp->ddp_lsat.sat_port = ATADDR_ANYPORT;

	ddp->ddp_next = ddpcb;
	ddp->ddp_prev = NULL;
	ddp->ddp_pprev = NULL;
	ddp->ddp_pnext = NULL;
	if (ddpcb) {
		ddpcb->ddp_prev = ddp;
	}
	ddpcb = ddp;

	ddp->ddp_socket = so;
	so->so_pcb = ddp;
	return 0;
}

static void
ddp_detach(struct socket *so)
{
	struct ddpcb *ddp = sotoddpcb(so);

	soisdisconnected(so);
	so->so_pcb = NULL;
	/* sofree drops the lock */
	sofree(so);
	mutex_enter(softnet_lock);

	/* remove ddp from ddp_ports list */
	if (ddp->ddp_lsat.sat_port != ATADDR_ANYPORT &&
	    ddp_ports[ddp->ddp_lsat.sat_port - 1] != NULL) {
		if (ddp->ddp_pprev != NULL) {
			ddp->ddp_pprev->ddp_pnext = ddp->ddp_pnext;
		} else {
			ddp_ports[ddp->ddp_lsat.sat_port - 1] = ddp->ddp_pnext;
		}
		if (ddp->ddp_pnext != NULL) {
			ddp->ddp_pnext->ddp_pprev = ddp->ddp_pprev;
		}
	}
	rtcache_free(&ddp->ddp_route);
	if (ddp->ddp_prev) {
		ddp->ddp_prev->ddp_next = ddp->ddp_next;
	} else {
		ddpcb = ddp->ddp_next;
	}
	if (ddp->ddp_next) {
		ddp->ddp_next->ddp_prev = ddp->ddp_prev;
	}
	kmem_free(ddp, sizeof(*ddp));
}

static int
ddp_accept(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
ddp_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	KASSERT(solocked(so));
	KASSERT(sotoddpcb(so) != NULL);

	return at_pcbsetaddr(sotoddpcb(so), (struct sockaddr_at *)nam);
}

static int
ddp_listen(struct socket *so, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
ddp_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct ddpcb *ddp = sotoddpcb(so);
	int error = 0;

	KASSERT(solocked(so));
	KASSERT(ddp != NULL);
	KASSERT(nam != NULL);

	if (ddp->ddp_fsat.sat_port != ATADDR_ANYPORT)
		return EISCONN;
	error = at_pcbconnect(ddp, (struct sockaddr_at *)nam);
	if (error == 0)
		soisconnected(so);

	return error;
}

static int
ddp_connect2(struct socket *so, struct socket *so2)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
ddp_disconnect(struct socket *so)
{
	struct ddpcb *ddp = sotoddpcb(so);

	KASSERT(solocked(so));
	KASSERT(ddp != NULL);

	if (ddp->ddp_fsat.sat_addr.s_node == ATADDR_ANYNODE)
		return ENOTCONN;

	at_pcbdisconnect(ddp);
	soisdisconnected(so);
	return 0;
}

static int
ddp_shutdown(struct socket *so)
{
	KASSERT(solocked(so));

	socantsendmore(so);
	return 0;
}

static int
ddp_abort(struct socket *so)
{
	KASSERT(solocked(so));

	soisdisconnected(so);
	ddp_detach(so);
	return 0;
}

static int
ddp_ioctl(struct socket *so, u_long cmd, void *addr, struct ifnet *ifp)
{
	return at_control(cmd, addr, ifp);
}

static int
ddp_stat(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	/* stat: don't bother with a blocksize. */
	return 0;
}

static int
ddp_peeraddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
ddp_sockaddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));
	KASSERT(sotoddpcb(so) != NULL);
	KASSERT(nam != NULL);

	at_sockaddr(sotoddpcb(so), (struct sockaddr_at *)nam);
	return 0;
}

static int
ddp_rcvd(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
ddp_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
ddp_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	struct ddpcb *ddp = sotoddpcb(so);
	int error = 0;
	int s = 0; /* XXX gcc 4.8 warns on sgimips */

	KASSERT(solocked(so));
	KASSERT(ddp != NULL);

	if (nam) {
		if (ddp->ddp_fsat.sat_port != ATADDR_ANYPORT)
			return EISCONN;
		s = splnet();
		error = at_pcbconnect(ddp, (struct sockaddr_at *)nam);
		if (error) {
			splx(s);
			return error;
		}
	} else {
		if (ddp->ddp_fsat.sat_port == ATADDR_ANYPORT)
			return ENOTCONN;
	}

	error = ddp_output(m, ddp);
	m = NULL;
	if (nam) {
		at_pcbdisconnect(ddp);
		splx(s);
	}

	return error;
}

static int
ddp_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	if (m)
		m_freem(m);

	return EOPNOTSUPP;
}

static int
ddp_purgeif(struct socket *so, struct ifnet *ifp)
{

	mutex_enter(softnet_lock);
	at_purgeif(ifp);
	mutex_exit(softnet_lock);

	return 0;
}

/*
 * For the moment, this just find the pcb with the correct local address.
 * In the future, this will actually do some real searching, so we can use
 * the sender's address to do de-multiplexing on a single port to many
 * sockets (pcbs).
 */
struct ddpcb   *
ddp_search(
    struct sockaddr_at *from,
    struct sockaddr_at *to,
    struct at_ifaddr *aa)
{
	struct ddpcb   *ddp;

	/*
         * Check for bad ports.
         */
	if (to->sat_port < ATPORT_FIRST || to->sat_port >= ATPORT_LAST)
		return NULL;

	/*
         * Make sure the local address matches the sent address.  What about
         * the interface?
         */
	for (ddp = ddp_ports[to->sat_port - 1]; ddp; ddp = ddp->ddp_pnext) {
		/* XXX should we handle 0.YY? */

		/* XXXX.YY to socket on destination interface */
		if (to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net &&
		    to->sat_addr.s_node == ddp->ddp_lsat.sat_addr.s_node) {
			break;
		}
		/* 0.255 to socket on receiving interface */
		if (to->sat_addr.s_node == ATADDR_BCAST &&
		    (to->sat_addr.s_net == 0 ||
		    to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net) &&
		ddp->ddp_lsat.sat_addr.s_net == AA_SAT(aa)->sat_addr.s_net) {
			break;
		}
		/* XXXX.0 to socket on destination interface */
		if (to->sat_addr.s_net == aa->aa_firstnet &&
		    to->sat_addr.s_node == 0 &&
		    ntohs(ddp->ddp_lsat.sat_addr.s_net) >=
		    ntohs(aa->aa_firstnet) &&
		    ntohs(ddp->ddp_lsat.sat_addr.s_net) <=
		    ntohs(aa->aa_lastnet)) {
			break;
		}
	}
	return (ddp);
}

/*
 * Initialize all the ddp & appletalk stuff
 */
void
ddp_init(void)
{

	ddpstat_percpu = percpu_alloc(sizeof(uint64_t) * DDP_NSTATS);

	TAILQ_INIT(&at_ifaddr);
	atintrq1.ifq_maxlen = IFQ_MAXLEN;
	atintrq2.ifq_maxlen = IFQ_MAXLEN;

	MOWNER_ATTACH(&atalk_tx_mowner);
	MOWNER_ATTACH(&atalk_rx_mowner);
	MOWNER_ATTACH(&aarp_mowner);
}

PR_WRAP_USRREQS(ddp)
#define	ddp_attach	ddp_attach_wrapper
#define	ddp_detach	ddp_detach_wrapper
#define	ddp_accept	ddp_accept_wrapper
#define	ddp_bind	ddp_bind_wrapper
#define	ddp_listen	ddp_listen_wrapper
#define	ddp_connect	ddp_connect_wrapper
#define	ddp_connect2	ddp_connect2_wrapper
#define	ddp_disconnect	ddp_disconnect_wrapper
#define	ddp_shutdown	ddp_shutdown_wrapper
#define	ddp_abort	ddp_abort_wrapper
#define	ddp_ioctl	ddp_ioctl_wrapper
#define	ddp_stat	ddp_stat_wrapper
#define	ddp_peeraddr	ddp_peeraddr_wrapper
#define	ddp_sockaddr	ddp_sockaddr_wrapper
#define	ddp_rcvd	ddp_rcvd_wrapper
#define	ddp_recvoob	ddp_recvoob_wrapper
#define	ddp_send	ddp_send_wrapper
#define	ddp_sendoob	ddp_sendoob_wrapper
#define	ddp_purgeif	ddp_purgeif_wrapper

const struct pr_usrreqs ddp_usrreqs = {
	.pr_attach	= ddp_attach,
	.pr_detach	= ddp_detach,
	.pr_accept	= ddp_accept,
	.pr_bind	= ddp_bind,
	.pr_listen	= ddp_listen,
	.pr_connect	= ddp_connect,
	.pr_connect2	= ddp_connect2,
	.pr_disconnect	= ddp_disconnect,
	.pr_shutdown	= ddp_shutdown,
	.pr_abort	= ddp_abort,
	.pr_ioctl	= ddp_ioctl,
	.pr_stat	= ddp_stat,
	.pr_peeraddr	= ddp_peeraddr,
	.pr_sockaddr	= ddp_sockaddr,
	.pr_rcvd	= ddp_rcvd,
	.pr_recvoob	= ddp_recvoob,
	.pr_send	= ddp_send,
	.pr_sendoob	= ddp_sendoob,
	.pr_purgeif	= ddp_purgeif,
};

static int
sysctl_net_atalk_ddp_stats(SYSCTLFN_ARGS)
{

	return (NETSTAT_SYSCTL(ddpstat_percpu, DDP_NSTATS));
}

/*
 * Sysctl for DDP variables.
 */
SYSCTL_SETUP(sysctl_net_atalk_ddp_setup, "sysctl net.atalk.ddp subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "atalk", NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_APPLETALK, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ddp",
		       SYSCTL_DESCR("DDP related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_APPLETALK, ATPROTO_DDP, CTL_EOL);
	
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "stats",
		       SYSCTL_DESCR("DDP statistics"),
		       sysctl_net_atalk_ddp_stats, 0, NULL, 0,
		       CTL_NET, PF_APPLETALK, ATPROTO_DDP, CTL_CREATE,
		       CTL_EOL);
}
