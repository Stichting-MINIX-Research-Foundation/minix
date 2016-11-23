/*	$NetBSD: sco_socket.c,v 1.37 2015/05/02 17:18:03 rtr Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
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
__KERNEL_RCSID(0, "$NetBSD: sco_socket.c,v 1.37 2015/05/02 17:18:03 rtr Exp $");

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
#include <netbt/hci.h>
#include <netbt/sco.h>

/*******************************************************************************
 *
 * SCO SOCK_SEQPACKET sockets - low latency audio data
 */

static void sco_connecting(void *);
static void sco_connected(void *);
static void sco_disconnected(void *, int);
static void *sco_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void sco_complete(void *, int);
static void sco_linkmode(void *, int);
static void sco_input(void *, struct mbuf *);

static const struct btproto sco_proto = {
	sco_connecting,
	sco_connected,
	sco_disconnected,
	sco_newconn,
	sco_complete,
	sco_linkmode,
	sco_input,
};

int sco_sendspace = 4096;
int sco_recvspace = 4096;

static int
sco_attach(struct socket *so, int proto)
{
	int error;

	KASSERT(so->so_pcb == NULL);

	if (so->so_lock == NULL) {
		mutex_obj_hold(bt_lock);
		so->so_lock = bt_lock;
		solock(so);
	}
	KASSERT(solocked(so));

	error = soreserve(so, sco_sendspace, sco_recvspace);
	if (error) {
		return error;
	}
	return sco_attach_pcb((struct sco_pcb **)&so->so_pcb, &sco_proto, so);
}

static void
sco_detach(struct socket *so)
{
	KASSERT(so->so_pcb != NULL);
	sco_detach_pcb((struct sco_pcb **)&so->so_pcb);
	KASSERT(so->so_pcb == NULL);
}

static int
sco_accept(struct socket *so, struct sockaddr *nam)
{
	struct sco_pcb *pcb = so->so_pcb;

	KASSERT(solocked(so));
	KASSERT(nam != NULL);

	if (pcb == NULL)
		return EINVAL;

	return sco_peeraddr_pcb(pcb, (struct sockaddr_bt *)nam);
}

static int
sco_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct sco_pcb *pcb = so->so_pcb;
	struct sockaddr_bt *sa = (struct sockaddr_bt *)nam;

	KASSERT(solocked(so));
	KASSERT(nam != NULL);

	if (pcb == NULL)
		return EINVAL;

	if (sa->bt_len != sizeof(struct sockaddr_bt))
		return EINVAL;

	if (sa->bt_family != AF_BLUETOOTH)
		return EAFNOSUPPORT;

	return sco_bind_pcb(pcb, sa);
}

static int
sco_listen(struct socket *so, struct lwp *l)
{
	struct sco_pcb *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	return sco_listen_pcb(pcb);
}

static int
sco_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct sco_pcb *pcb = so->so_pcb;
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
	return sco_connect_pcb(pcb, sa);
}

static int
sco_connect2(struct socket *so, struct socket *so2)
{
	struct sco_pcb *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	return EOPNOTSUPP;
}

static int
sco_disconnect(struct socket *so)
{
	struct sco_pcb *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	soisdisconnecting(so);
	return sco_disconnect_pcb(pcb, so->so_linger);
}

static int
sco_shutdown(struct socket *so)
{
	KASSERT(solocked(so));

	socantsendmore(so);
	return 0;
}

static int
sco_abort(struct socket *so)
{
	struct sco_pcb *pcb = so->so_pcb;

	KASSERT(solocked(so));

	if (pcb == NULL)
		return EINVAL;

	sco_disconnect_pcb(pcb, 0);
	soisdisconnected(so);
	sco_detach(so);
	return 0;
}

static int
sco_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	return EOPNOTSUPP;
}

static int
sco_stat(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	return 0;
}

static int
sco_peeraddr(struct socket *so, struct sockaddr *nam)
{
	struct sco_pcb *pcb = (struct sco_pcb *)so->so_pcb;

	KASSERT(solocked(so));
	KASSERT(pcb != NULL);
	KASSERT(nam != NULL);

	return sco_peeraddr_pcb(pcb, (struct sockaddr_bt *)nam);
}

static int
sco_sockaddr(struct socket *so, struct sockaddr *nam)
{
	struct sco_pcb *pcb = (struct sco_pcb *)so->so_pcb;

	KASSERT(solocked(so));
	KASSERT(pcb != NULL);
	KASSERT(nam != NULL);

	return sco_sockaddr_pcb(pcb, (struct sockaddr_bt *)nam);
}

static int
sco_rcvd(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
sco_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
sco_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	struct sco_pcb *pcb = so->so_pcb;
	int err = 0;
	struct mbuf *m0;

	KASSERT(solocked(so));
	KASSERT(m != NULL);

	if (control) /* no use for that */
		m_freem(control);

	if (pcb == NULL) {
		err = EINVAL;
		goto release;
	}

	if (m->m_pkthdr.len == 0)
		goto release;

	if (m->m_pkthdr.len > pcb->sp_mtu) {
		err = EMSGSIZE;
		goto release;
	}

	m0 = m_copypacket(m, M_DONTWAIT);
	if (m0 == NULL) {
		err = ENOMEM;
		goto release;
	}

	sbappendrecord(&so->so_snd, m);
	return sco_send_pcb(pcb, m0);

release:
	m_freem(m);
	return err;
}

static int
sco_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	if (m)
		m_freem(m);
	if (control)
		m_freem(control);

	return EOPNOTSUPP;
}

static int
sco_purgeif(struct socket *so, struct ifnet *ifp)
{

	return EOPNOTSUPP;
}

/*
 * get/set socket options
 */
int
sco_ctloutput(int req, struct socket *so, struct sockopt *sopt)
{
	struct sco_pcb *pcb = (struct sco_pcb *)so->so_pcb;
	int err = 0;

	DPRINTFN(2, "req %s\n", prcorequests[req]);

	if (pcb == NULL)
		return EINVAL;

	if (sopt->sopt_level != BTPROTO_SCO)
		return ENOPROTOOPT;

	switch(req) {
	case PRCO_GETOPT:
		err = sco_getopt(pcb, sopt);
		break;

	case PRCO_SETOPT:
		err = sco_setopt(pcb, sopt);
		break;

	default:
		err = ENOPROTOOPT;
		break;
	}

	return err;
}

/*****************************************************************************
 *
 *	SCO Protocol socket callbacks
 *
 */
static void
sco_connecting(void *arg)
{
	struct socket *so = arg;

	DPRINTF("Connecting\n");
	soisconnecting(so);
}

static void
sco_connected(void *arg)
{
	struct socket *so = arg;

	DPRINTF("Connected\n");
	soisconnected(so);
}

static void
sco_disconnected(void *arg, int err)
{
	struct socket *so = arg;

	DPRINTF("Disconnected (%d)\n", err);

	so->so_error = err;
	soisdisconnected(so);
}

static void *
sco_newconn(void *arg, struct sockaddr_bt *laddr,
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

static void
sco_complete(void *arg, int num)
{
	struct socket *so = arg;

	while (num-- > 0)
		sbdroprecord(&so->so_snd);

	sowwakeup(so);
}

static void
sco_linkmode(void *arg, int mode)
{
}

static void
sco_input(void *arg, struct mbuf *m)
{
	struct socket *so = arg;

	/*
	 * since this data is time sensitive, if the buffer
	 * is full we just dump data until the latest one
	 * will fit.
	 */

	while (m->m_pkthdr.len > sbspace(&so->so_rcv))
		sbdroprecord(&so->so_rcv);

	DPRINTFN(10, "received %d bytes\n", m->m_pkthdr.len);

	sbappendrecord(&so->so_rcv, m);
	sorwakeup(so);
}

PR_WRAP_USRREQS(sco)

#define	sco_attach		sco_attach_wrapper
#define	sco_detach		sco_detach_wrapper
#define	sco_accept		sco_accept_wrapper
#define	sco_bind		sco_bind_wrapper
#define	sco_listen		sco_listen_wrapper
#define	sco_connect		sco_connect_wrapper
#define	sco_connect2		sco_connect2_wrapper
#define	sco_disconnect		sco_disconnect_wrapper
#define	sco_shutdown		sco_shutdown_wrapper
#define	sco_abort		sco_abort_wrapper
#define	sco_ioctl		sco_ioctl_wrapper
#define	sco_stat		sco_stat_wrapper
#define	sco_peeraddr		sco_peeraddr_wrapper
#define	sco_sockaddr		sco_sockaddr_wrapper
#define	sco_rcvd		sco_rcvd_wrapper
#define	sco_recvoob		sco_recvoob_wrapper
#define	sco_send		sco_send_wrapper
#define	sco_sendoob		sco_sendoob_wrapper
#define	sco_purgeif		sco_purgeif_wrapper

const struct pr_usrreqs sco_usrreqs = {
	.pr_attach	= sco_attach,
	.pr_detach	= sco_detach,
	.pr_accept	= sco_accept,
	.pr_bind	= sco_bind,
	.pr_listen	= sco_listen,
	.pr_connect	= sco_connect,
	.pr_connect2	= sco_connect2,
	.pr_disconnect	= sco_disconnect,
	.pr_shutdown	= sco_shutdown,
	.pr_abort	= sco_abort,
	.pr_ioctl	= sco_ioctl,
	.pr_stat	= sco_stat,
	.pr_peeraddr	= sco_peeraddr,
	.pr_sockaddr	= sco_sockaddr,
	.pr_rcvd	= sco_rcvd,
	.pr_recvoob	= sco_recvoob,
	.pr_send	= sco_send,
	.pr_sendoob	= sco_sendoob,
	.pr_purgeif	= sco_purgeif,
};
