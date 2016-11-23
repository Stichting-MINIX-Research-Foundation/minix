/*	$NetBSD: rfcomm_socket.c,v 1.37 2015/05/02 17:18:03 rtr Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rfcomm_socket.c,v 1.37 2015/05/02 17:18:03 rtr Exp $");

/* load symbolic names */
#ifdef BLUETOOTH_DEBUG
#define PRUREQUESTS
#define PRCOREQUESTS
#endif

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/rfcomm.h>

/****************************************************************************
 *
 *	RFCOMM SOCK_STREAM Sockets - serial line emulation
 *
 */

static void rfcomm_connecting(void *);
static void rfcomm_connected(void *);
static void rfcomm_disconnected(void *, int);
static void *rfcomm_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void rfcomm_complete(void *, int);
static void rfcomm_linkmode(void *, int);
static void rfcomm_input(void *, struct mbuf *);

static const struct btproto rfcomm_proto = {
	rfcomm_connecting,
	rfcomm_connected,
	rfcomm_disconnected,
	rfcomm_newconn,
	rfcomm_complete,
	rfcomm_linkmode,
	rfcomm_input,
};

/* sysctl variables */
int rfcomm_sendspace = 4096;
int rfcomm_recvspace = 4096;

static int
rfcomm_attach(struct socket *so, int proto)
{
	int error;

	KASSERT(so->so_pcb == NULL);

	if (so->so_lock == NULL) {
		mutex_obj_hold(bt_lock);
		so->so_lock = bt_lock;
		solock(so);
	}
	KASSERT(solocked(so));

	/*
	 * Since we have nothing to add, we attach the DLC
	 * structure directly to our PCB pointer.
	 */
	error = soreserve(so, rfcomm_sendspace, rfcomm_recvspace);
	if (error)
		return error;

	error = rfcomm_attach_pcb((struct rfcomm_dlc **)&so->so_pcb,
				&rfcomm_proto, so);
	if (error)
		return error;

	error = rfcomm_rcvd_pcb(so->so_pcb, sbspace(&so->so_rcv));
	if (error) {
		rfcomm_detach_pcb((struct rfcomm_dlc **)&so->so_pcb);
		return error;
	}
	return 0;
}

static void
rfcomm_detach(struct socket *so)
{
	KASSERT(so->so_pcb != NULL);
	rfcomm_detach_pcb((struct rfcomm_dlc **)&so->so_pcb);
	KASSERT(so->so_pcb == NULL);
}

static int
rfcomm_accept(struct socket *so, struct sockaddr *nam)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));
	KASSERT(nam != NULL);

	if (pcb == NULL)
		return EINVAL;

	return rfcomm_peeraddr_pcb(pcb, (struct sockaddr_bt *)nam);
}

static int
rfcomm_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct rfcomm_dlc *pcb = so->so_pcb;
	struct sockaddr_bt *sa = (struct sockaddr_bt *)nam;

	KASSERT(solocked(so));
	KASSERT(nam != NULL);

	if (pcb == NULL)
		return EINVAL;

	if (sa->bt_len != sizeof(struct sockaddr_bt))
		return EINVAL;

	if (sa->bt_family != AF_BLUETOOTH)
		return EAFNOSUPPORT;

	return rfcomm_bind_pcb(pcb, sa);
}

static int
rfcomm_listen(struct socket *so, struct lwp *l)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	return rfcomm_listen_pcb(pcb);
}

static int
rfcomm_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct rfcomm_dlc *pcb = so->so_pcb;
	struct sockaddr_bt *sa = (struct sockaddr_bt *)nam;

	KASSERT(solocked(so));
	KASSERT(nam != NULL);

	if (pcb == NULL)
		return EINVAL;

	if (sa->bt_len != sizeof(struct sockaddr_bt))
		return EINVAL;

	if (sa->bt_family != AF_BLUETOOTH)
		return EAFNOSUPPORT;

	soisconnecting(so);
	return rfcomm_connect_pcb(pcb, sa);
}

static int
rfcomm_connect2(struct socket *so, struct socket *so2)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	return EOPNOTSUPP;
}

static int
rfcomm_disconnect(struct socket *so)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	soisdisconnecting(so);
	return rfcomm_disconnect_pcb(pcb, so->so_linger);
}

static int
rfcomm_shutdown(struct socket *so)
{
	KASSERT(solocked(so));

	socantsendmore(so);
	return 0;
}

static int
rfcomm_abort(struct socket *so)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	rfcomm_disconnect_pcb(pcb, 0);
	soisdisconnected(so);
	rfcomm_detach(so);
	return 0;
}

static int
rfcomm_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	return EPASSTHROUGH;
}

static int
rfcomm_stat(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	return 0;
}

static int
rfcomm_peeraddr(struct socket *so, struct sockaddr *nam)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));
	KASSERT(pcb != NULL);
	KASSERT(nam != NULL);

	return rfcomm_peeraddr_pcb(pcb, (struct sockaddr_bt *)nam);
}

static int
rfcomm_sockaddr(struct socket *so, struct sockaddr *nam)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));
	KASSERT(pcb != NULL);
	KASSERT(nam != NULL);

	return rfcomm_sockaddr_pcb(pcb, (struct sockaddr_bt *)nam);
}

static int
rfcomm_rcvd(struct socket *so, int flags, struct lwp *l)
{
	struct rfcomm_dlc *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	return rfcomm_rcvd_pcb(pcb, sbspace(&so->so_rcv));
}

static int
rfcomm_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
rfcomm_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	struct rfcomm_dlc *pcb = so->so_pcb;
	int err = 0;
	struct mbuf *m0;

	KASSERT(solocked(so));
	KASSERT(m != NULL);

	if (control)	/* no use for that */
		m_freem(control);

	if (pcb == NULL) {
		err = EINVAL;
		goto release;
	}

	m0 = m_copypacket(m, M_DONTWAIT);
	if (m0 == NULL) {
		err = ENOMEM;
		goto release;
	}

	sbappendstream(&so->so_snd, m);
	return rfcomm_send_pcb(pcb, m0);

release:
	m_freem(m);
	return err;
}

static int
rfcomm_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	if (m)
		m_freem(m);
	if (control)
		m_freem(control);

	return EOPNOTSUPP;
}

static int
rfcomm_purgeif(struct socket *so, struct ifnet *ifp)
{

	return EOPNOTSUPP;
}

/*
 * rfcomm_ctloutput(req, socket, sockopt)
 *
 */
int
rfcomm_ctloutput(int req, struct socket *so, struct sockopt *sopt)
{
	struct rfcomm_dlc *pcb = so->so_pcb;
	int err = 0;

	DPRINTFN(2, "%s\n", prcorequests[req]);

	if (pcb == NULL)
		return EINVAL;

	if (sopt->sopt_level != BTPROTO_RFCOMM)
		return ENOPROTOOPT;

	switch(req) {
	case PRCO_GETOPT:
		err = rfcomm_getopt(pcb, sopt);
		break;

	case PRCO_SETOPT:
		err = rfcomm_setopt(pcb, sopt);
		break;

	default:
		err = ENOPROTOOPT;
		break;
	}

	return err;
}

/**********************************************************************
 *
 * RFCOMM callbacks
 */

static void
rfcomm_connecting(void *arg)
{
	/* struct socket *so = arg; */

	KASSERT(arg != NULL);
	DPRINTF("Connecting\n");
}

static void
rfcomm_connected(void *arg)
{
	struct socket *so = arg;

	KASSERT(so != NULL);
	DPRINTF("Connected\n");
	soisconnected(so);
}

static void
rfcomm_disconnected(void *arg, int err)
{
	struct socket *so = arg;

	KASSERT(so != NULL);
	DPRINTF("Disconnected\n");

	so->so_error = err;
	soisdisconnected(so);
}

static void *
rfcomm_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct socket *so = arg;

	DPRINTF("New Connection\n");
	so = sonewconn(so, false);
	if (so == NULL)
		return NULL;

	soisconnecting(so);

	return so->so_pcb;
}

/*
 * rfcomm_complete(rfcomm_dlc, length)
 *
 * length bytes are sent and may be removed from socket buffer
 */
static void
rfcomm_complete(void *arg, int length)
{
	struct socket *so = arg;

	sbdrop(&so->so_snd, length);
	sowwakeup(so);
}

/*
 * rfcomm_linkmode(rfcomm_dlc, new)
 *
 * link mode change notification.
 */
static void
rfcomm_linkmode(void *arg, int new)
{
	struct socket *so = arg;
	struct sockopt sopt;
	int mode;

	DPRINTF("auth %s, encrypt %s, secure %s\n",
		(new & RFCOMM_LM_AUTH ? "on" : "off"),
		(new & RFCOMM_LM_ENCRYPT ? "on" : "off"),
		(new & RFCOMM_LM_SECURE ? "on" : "off"));

	sockopt_init(&sopt, BTPROTO_RFCOMM, SO_RFCOMM_LM, 0);
	(void)rfcomm_getopt(so->so_pcb, &sopt);
	(void)sockopt_getint(&sopt, &mode);
	sockopt_destroy(&sopt);

	if (((mode & RFCOMM_LM_AUTH) && !(new & RFCOMM_LM_AUTH))
	    || ((mode & RFCOMM_LM_ENCRYPT) && !(new & RFCOMM_LM_ENCRYPT))
	    || ((mode & RFCOMM_LM_SECURE) && !(new & RFCOMM_LM_SECURE)))
		rfcomm_disconnect_pcb(so->so_pcb, 0);
}

/*
 * rfcomm_input(rfcomm_dlc, mbuf)
 */
static void
rfcomm_input(void *arg, struct mbuf *m)
{
	struct socket *so = arg;

	KASSERT(so != NULL);

	if (m->m_pkthdr.len > sbspace(&so->so_rcv)) {
		printf("%s: %d bytes dropped (socket buffer full)\n",
			__func__, m->m_pkthdr.len);
		m_freem(m);
		return;
	}

	DPRINTFN(10, "received %d bytes\n", m->m_pkthdr.len);

	sbappendstream(&so->so_rcv, m);
	sorwakeup(so);
}

PR_WRAP_USRREQS(rfcomm)

#define	rfcomm_attach		rfcomm_attach_wrapper
#define	rfcomm_detach		rfcomm_detach_wrapper
#define	rfcomm_accept		rfcomm_accept_wrapper
#define	rfcomm_bind		rfcomm_bind_wrapper
#define	rfcomm_listen		rfcomm_listen_wrapper
#define	rfcomm_connect		rfcomm_connect_wrapper
#define	rfcomm_connect2		rfcomm_connect2_wrapper
#define	rfcomm_disconnect	rfcomm_disconnect_wrapper
#define	rfcomm_shutdown		rfcomm_shutdown_wrapper
#define	rfcomm_abort		rfcomm_abort_wrapper
#define	rfcomm_ioctl		rfcomm_ioctl_wrapper
#define	rfcomm_stat		rfcomm_stat_wrapper
#define	rfcomm_peeraddr		rfcomm_peeraddr_wrapper
#define	rfcomm_sockaddr		rfcomm_sockaddr_wrapper
#define	rfcomm_rcvd		rfcomm_rcvd_wrapper
#define	rfcomm_recvoob		rfcomm_recvoob_wrapper
#define	rfcomm_send		rfcomm_send_wrapper
#define	rfcomm_sendoob		rfcomm_sendoob_wrapper
#define	rfcomm_purgeif		rfcomm_purgeif_wrapper

const struct pr_usrreqs rfcomm_usrreqs = {
	.pr_attach	= rfcomm_attach,
	.pr_detach	= rfcomm_detach,
	.pr_accept	= rfcomm_accept,
	.pr_bind	= rfcomm_bind,
	.pr_listen	= rfcomm_listen,
	.pr_connect	= rfcomm_connect,
	.pr_connect2	= rfcomm_connect2,
	.pr_disconnect	= rfcomm_disconnect,
	.pr_shutdown	= rfcomm_shutdown,
	.pr_abort	= rfcomm_abort,
	.pr_ioctl	= rfcomm_ioctl,
	.pr_stat	= rfcomm_stat,
	.pr_peeraddr	= rfcomm_peeraddr,
	.pr_sockaddr	= rfcomm_sockaddr,
	.pr_rcvd	= rfcomm_rcvd,
	.pr_recvoob	= rfcomm_recvoob,
	.pr_send	= rfcomm_send,
	.pr_sendoob	= rfcomm_sendoob,
	.pr_purgeif	= rfcomm_purgeif,
};
