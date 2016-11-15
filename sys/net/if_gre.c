/*	$NetBSD: if_gre.c,v 1.167 2015/08/24 22:21:26 pooka Exp $ */

/*
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
 *
 * GRE over UDP/IPv4/IPv6 sockets contributed by David Young <dyoung@NetBSD.org>
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
 *
 * This material is based upon work partially supported by NSF
 * under Contract No. NSF CNS-0626584.
 */

/*
 * Encapsulate L3 protocols into IP
 * See RFC 1701 and 1702 for more details.
 * If_gre is compatible with Cisco GRE tunnels, so you can
 * have a NetBSD box as the other end of a tunnel interface of a Cisco
 * router. See gre(4) for more details.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_gre.c,v 1.167 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_atalk.h"
#include "opt_gre.h"
#include "opt_inet.h"
#include "opt_mpls.h"
#endif

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/mallocvar.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/kthread.h>

#include <sys/cpu.h>

#include <net/ethertypes.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* we always need this for sizeof(struct ip) */

#ifdef INET
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#include <netmpls/mpls_var.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>
#endif

#include <sys/time.h>
#include <net/bpf.h>

#include <net/if_gre.h>

#include <compat/sys/socket.h>
#include <compat/sys/sockio.h>

#include "ioconf.h"

/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU 1476

#ifdef GRE_DEBUG
int gre_debug = 0;
#define	GRE_DPRINTF(__sc, ...)						\
	do {								\
		if (__predict_false(gre_debug ||			\
		    ((__sc)->sc_if.if_flags & IFF_DEBUG) != 0)) {	\
			printf("%s.%d: ", __func__, __LINE__);		\
			printf(__VA_ARGS__);				\
		}							\
	} while (/*CONSTCOND*/0)
#else
#define	GRE_DPRINTF(__sc, __fmt, ...)	do { } while (/*CONSTCOND*/0)
#endif /* GRE_DEBUG */

int ip_gre_ttl = GRE_TTL;

static int gre_clone_create(struct if_clone *, int);
static int gre_clone_destroy(struct ifnet *);

static struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy);

static int gre_input(struct gre_softc *, struct mbuf *, int,
    const struct gre_h *);
static bool gre_is_nullconf(const struct gre_soparm *);
static int gre_output(struct ifnet *, struct mbuf *,
			   const struct sockaddr *, struct rtentry *);
static int gre_ioctl(struct ifnet *, u_long, void *);
static int gre_getsockname(struct socket *, struct sockaddr *);
static int gre_getpeername(struct socket *, struct sockaddr *);
static int gre_getnames(struct socket *, struct lwp *,
    struct sockaddr_storage *, struct sockaddr_storage *);
static void gre_clearconf(struct gre_soparm *, bool);
static int gre_soreceive(struct socket *, struct mbuf **);
static int gre_sosend(struct socket *, struct mbuf *);
static struct socket *gre_reconf(struct gre_softc *, const struct gre_soparm *);

static bool gre_fp_send(struct gre_softc *, enum gre_msg, file_t *);
static bool gre_fp_recv(struct gre_softc *);
static void gre_fp_recvloop(void *);

static void
gre_bufq_init(struct gre_bufq *bq, size_t len0)
{
	memset(bq, 0, sizeof(*bq));
	bq->bq_q = pcq_create(len0, KM_SLEEP);
	KASSERT(bq->bq_q != NULL);
}

static struct mbuf *
gre_bufq_dequeue(struct gre_bufq *bq)
{
	return pcq_get(bq->bq_q);
}

static void
gre_bufq_purge(struct gre_bufq *bq)
{
	struct mbuf *m;

	while ((m = gre_bufq_dequeue(bq)) != NULL)
		m_freem(m);
}

static void
gre_bufq_destroy(struct gre_bufq *bq)
{
	gre_bufq_purge(bq);
	pcq_destroy(bq->bq_q);
}

static int
gre_bufq_enqueue(struct gre_bufq *bq, struct mbuf *m)
{
	KASSERT(bq->bq_q != NULL);

	if (!pcq_put(bq->bq_q, m)) {
		bq->bq_drops++;
		return ENOBUFS;
	}
	return 0;
}

static void
greintr(void *arg)
{
	struct gre_softc *sc = (struct gre_softc *)arg;
	struct socket *so = sc->sc_soparm.sp_so;
	int rc;
	struct mbuf *m;

	KASSERT(so != NULL);

	sc->sc_send_ev.ev_count++;
	GRE_DPRINTF(sc, "enter\n");
	while ((m = gre_bufq_dequeue(&sc->sc_snd)) != NULL) {
		/* XXX handle ENOBUFS? */
		if ((rc = gre_sosend(so, m)) != 0)
			GRE_DPRINTF(sc, "gre_sosend failed %d\n", rc);
	}
}

/* Caller must hold sc->sc_mtx. */
static void
gre_fp_wait(struct gre_softc *sc)
{
	sc->sc_fp_waiters++;
	cv_wait(&sc->sc_fp_condvar, &sc->sc_mtx);
	sc->sc_fp_waiters--;
}

static void
gre_evcnt_detach(struct gre_softc *sc)
{
	evcnt_detach(&sc->sc_recv_ev);
	evcnt_detach(&sc->sc_block_ev);
	evcnt_detach(&sc->sc_error_ev);
	evcnt_detach(&sc->sc_pullup_ev);
	evcnt_detach(&sc->sc_unsupp_ev);

	evcnt_detach(&sc->sc_send_ev);
	evcnt_detach(&sc->sc_oflow_ev);
}

static void
gre_evcnt_attach(struct gre_softc *sc)
{
	evcnt_attach_dynamic(&sc->sc_recv_ev, EVCNT_TYPE_MISC,
	    NULL, sc->sc_if.if_xname, "recv");
	evcnt_attach_dynamic(&sc->sc_block_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "would block");
	evcnt_attach_dynamic(&sc->sc_error_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "error");
	evcnt_attach_dynamic(&sc->sc_pullup_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "pullup failed");
	evcnt_attach_dynamic(&sc->sc_unsupp_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "unsupported");

	evcnt_attach_dynamic(&sc->sc_send_ev, EVCNT_TYPE_MISC,
	    NULL, sc->sc_if.if_xname, "send");
	evcnt_attach_dynamic(&sc->sc_oflow_ev, EVCNT_TYPE_MISC,
	    &sc->sc_send_ev, sc->sc_if.if_xname, "overflow");
}

static int
gre_clone_create(struct if_clone *ifc, int unit)
{
	int rc;
	struct gre_softc *sc;
	struct gre_soparm *sp;
	const struct sockaddr *any;

	if ((any = sockaddr_any_by_family(AF_INET)) == NULL &&
	    (any = sockaddr_any_by_family(AF_INET6)) == NULL)
		goto fail0;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	mutex_init(&sc->sc_mtx, MUTEX_DRIVER, IPL_SOFTNET);
	cv_init(&sc->sc_condvar, "gre wait");
	cv_init(&sc->sc_fp_condvar, "gre fp");

	if_initname(&sc->sc_if, ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_type = IFT_TUNNEL;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_hdrlen = sizeof(struct ip) + sizeof(struct gre_h);
	sc->sc_if.if_dlt = DLT_NULL;
	sc->sc_if.if_mtu = GREMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	sc->sc_if.if_output = gre_output;
	sc->sc_if.if_ioctl = gre_ioctl;
	sp = &sc->sc_soparm;
	sockaddr_copy(sstosa(&sp->sp_dst), sizeof(sp->sp_dst), any);
	sockaddr_copy(sstosa(&sp->sp_src), sizeof(sp->sp_src), any);
	sp->sp_proto = IPPROTO_GRE;
	sp->sp_type = SOCK_RAW;

	sc->sc_fd = -1;

	rc = kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL, gre_fp_recvloop, sc,
	    NULL, "%s", sc->sc_if.if_xname);
	if (rc)
		goto fail1;

	gre_evcnt_attach(sc);

	gre_bufq_init(&sc->sc_snd, 17);
	sc->sc_if.if_flags |= IFF_LINK0;
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
	bpf_attach(&sc->sc_if, DLT_NULL, sizeof(uint32_t));
	return 0;

fail1:	cv_destroy(&sc->sc_fp_condvar);
	cv_destroy(&sc->sc_condvar);
	mutex_destroy(&sc->sc_mtx);
	free(sc, M_DEVBUF);
fail0:	return -1;
}

static int
gre_clone_destroy(struct ifnet *ifp)
{
	int s;
	struct gre_softc *sc = ifp->if_softc;

	GRE_DPRINTF(sc, "\n");

	bpf_detach(ifp);
	s = splnet();
	if_detach(ifp);

	GRE_DPRINTF(sc, "\n");
	/* Note that we must not hold the mutex while we call gre_reconf(). */
	gre_reconf(sc, NULL);

	mutex_enter(&sc->sc_mtx);
	sc->sc_msg = GRE_M_STOP;
	cv_signal(&sc->sc_fp_condvar);
	while (sc->sc_fp_waiters > 0)
		cv_wait(&sc->sc_fp_condvar, &sc->sc_mtx);
	mutex_exit(&sc->sc_mtx);

	splx(s);

	cv_destroy(&sc->sc_condvar);
	cv_destroy(&sc->sc_fp_condvar);
	mutex_destroy(&sc->sc_mtx);
	gre_bufq_destroy(&sc->sc_snd);
	gre_evcnt_detach(sc);
	free(sc, M_DEVBUF);

	return 0;
}

static void
gre_receive(struct socket *so, void *arg, int events, int waitflag)
{
	struct gre_softc *sc = (struct gre_softc *)arg;
	int rc;
	const struct gre_h *gh;
	struct mbuf *m;

	GRE_DPRINTF(sc, "enter\n");

	sc->sc_recv_ev.ev_count++;

	rc = gre_soreceive(so, &m);
	/* TBD Back off if ECONNREFUSED (indicates
	 * ICMP Port Unreachable)?
	 */
	if (rc == EWOULDBLOCK) {
		GRE_DPRINTF(sc, "EWOULDBLOCK\n");
		sc->sc_block_ev.ev_count++;
		return;
	} else if (rc != 0 || m == NULL) {
		GRE_DPRINTF(sc, "%s: rc %d m %p\n",
		    sc->sc_if.if_xname, rc, (void *)m);
		sc->sc_error_ev.ev_count++;
		return;
	}
	if (m->m_len < sizeof(*gh) && (m = m_pullup(m, sizeof(*gh))) == NULL) {
		GRE_DPRINTF(sc, "m_pullup failed\n");
		sc->sc_pullup_ev.ev_count++;
		return;
	}
	gh = mtod(m, const struct gre_h *);

	if (gre_input(sc, m, 0, gh) == 0) {
		sc->sc_unsupp_ev.ev_count++;
		GRE_DPRINTF(sc, "dropping unsupported\n");
		m_freem(m);
	}
}

static void
gre_upcall_add(struct socket *so, void *arg)
{
	/* XXX What if the kernel already set an upcall? */
	KASSERT((so->so_rcv.sb_flags & SB_UPCALL) == 0);
	so->so_upcallarg = arg;
	so->so_upcall = gre_receive;
	so->so_rcv.sb_flags |= SB_UPCALL;
}

static void
gre_upcall_remove(struct socket *so)
{
	so->so_rcv.sb_flags &= ~SB_UPCALL;
	so->so_upcallarg = NULL;
	so->so_upcall = NULL;
}

static int
gre_socreate(struct gre_softc *sc, const struct gre_soparm *sp, int *fdout)
{
	int fd, rc;
	struct socket *so;
	struct sockaddr_big sbig;
	sa_family_t af;
	int val;

	GRE_DPRINTF(sc, "enter\n");

	af = sp->sp_src.ss_family;
	rc = fsocreate(af, NULL, sp->sp_type, sp->sp_proto, &fd);
	if (rc != 0) {
		GRE_DPRINTF(sc, "fsocreate failed\n");
		return rc;
	}

	if ((rc = fd_getsock(fd, &so)) != 0)
		return rc;

	memcpy(&sbig, &sp->sp_src, sizeof(sp->sp_src));
	if ((rc = sobind(so, (struct sockaddr *)&sbig, curlwp)) != 0) {
		GRE_DPRINTF(sc, "sobind failed\n");
		goto out;
	}

	memcpy(&sbig, &sp->sp_dst, sizeof(sp->sp_dst));
	solock(so);
	if ((rc = soconnect(so, (struct sockaddr *)&sbig, curlwp)) != 0) {
		GRE_DPRINTF(sc, "soconnect failed\n");
		sounlock(so);
		goto out;
	}
	sounlock(so);

	/* XXX convert to a (new) SOL_SOCKET call */
  	KASSERT(so->so_proto != NULL);
 	rc = so_setsockopt(curlwp, so, IPPROTO_IP, IP_TTL,
	    &ip_gre_ttl, sizeof(ip_gre_ttl));
  	if (rc != 0) {
 		GRE_DPRINTF(sc, "so_setsockopt ttl failed\n");
  		rc = 0;
  	}

 	val = 1;
 	rc = so_setsockopt(curlwp, so, SOL_SOCKET, SO_NOHEADER,
	    &val, sizeof(val));
  	if (rc != 0) {
 		GRE_DPRINTF(sc, "so_setsockopt SO_NOHEADER failed\n");
		rc = 0;
	}
out:
	if (rc != 0)
		fd_close(fd);
	else  {
		fd_putfile(fd);
		*fdout = fd;
	}

	return rc;
}

static int
gre_sosend(struct socket *so, struct mbuf *top)
{
	struct proc	*p;
	long		space, resid;
	int		error;
	struct lwp * const l = curlwp;

	p = l->l_proc;

	resid = top->m_pkthdr.len;
	if (p)
		l->l_ru.ru_msgsnd++;
#define	snderr(errno)	{ error = errno; goto release; }

	solock(so);
	if ((error = sblock(&so->so_snd, M_NOWAIT)) != 0)
		goto out;
	if (so->so_state & SS_CANTSENDMORE)
		snderr(EPIPE);
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		goto release;
	}
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
			snderr(ENOTCONN);
		} else {
			snderr(EDESTADDRREQ);
		}
	}
	space = sbspace(&so->so_snd);
	if (resid > so->so_snd.sb_hiwat)
		snderr(EMSGSIZE);
	if (space < resid)
		snderr(EWOULDBLOCK);
	/*
	 * Data is prepackaged in "top".
	 */
	if (so->so_state & SS_CANTSENDMORE)
		snderr(EPIPE);
	error = (*so->so_proto->pr_usrreqs->pr_send)(so,
	    top, NULL, NULL, l);
	top = NULL;
 release:
	sbunlock(&so->so_snd);
 out:
 	sounlock(so);
	if (top != NULL)
		m_freem(top);
	return error;
}

/* This is a stripped-down version of soreceive() that will never
 * block.  It will support SOCK_DGRAM sockets.  It may also support
 * SOCK_SEQPACKET sockets.
 */
static int
gre_soreceive(struct socket *so, struct mbuf **mp0)
{
	struct mbuf *m, **mp;
	int flags, len, error, type;
	const struct protosw	*pr;
	struct mbuf *nextrecord;

	KASSERT(mp0 != NULL);

	flags = MSG_DONTWAIT;
	pr = so->so_proto;
	mp = mp0;
	type = 0;

	*mp = NULL;

	KASSERT(pr->pr_flags & PR_ATOMIC);
 restart:
	if ((error = sblock(&so->so_rcv, M_NOWAIT)) != 0) {
		return error;
	}
	m = so->so_rcv.sb_mb;
	/*
	 * If we have less data than requested, do not block awaiting more.
	 */
	if (m == NULL) {
#ifdef DIAGNOSTIC
		if (so->so_rcv.sb_cc)
			panic("receive 1");
#endif
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
		} else if (so->so_state & SS_CANTRCVMORE)
			;
		else if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0
		      && (so->so_proto->pr_flags & PR_CONNREQUIRED))
			error = ENOTCONN;
		else
			error = EWOULDBLOCK;
		goto release;
	}
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * While we process the initial mbufs containing address and control
	 * info, we save a copy of m->m_nextpkt into nextrecord.
	 */
	if (curlwp != NULL)
		curlwp->l_ru.ru_msgrcv++;
	KASSERT(m == so->so_rcv.sb_mb);
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 1");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 1");
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
#endif
		sbfree(&so->so_rcv, m);
		MFREE(m, so->so_rcv.sb_mb);
		m = so->so_rcv.sb_mb;
	}
	while (m != NULL && m->m_type == MT_CONTROL && error == 0) {
		sbfree(&so->so_rcv, m);
		/*
		 * Dispose of any SCM_RIGHTS message that went
		 * through the read path rather than recv.
		 */
		if (pr->pr_domain->dom_dispose &&
		    mtod(m, struct cmsghdr *)->cmsg_type == SCM_RIGHTS)
			(*pr->pr_domain->dom_dispose)(m);
		MFREE(m, so->so_rcv.sb_mb);
		m = so->so_rcv.sb_mb;
	}

	/*
	 * If m is non-NULL, we have some data to read.  From now on,
	 * make sure to keep sb_lastrecord consistent when working on
	 * the last packet on the chain (nextrecord == NULL) and we
	 * change m->m_nextpkt.
	 */
	if (m != NULL) {
		m->m_nextpkt = nextrecord;
		/*
		 * If nextrecord == NULL (this is a single chain),
		 * then sb_lastrecord may not be valid here if m
		 * was changed earlier.
		 */
		if (nextrecord == NULL) {
			KASSERT(so->so_rcv.sb_mb == m);
			so->so_rcv.sb_lastrecord = m;
		}
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	} else {
		KASSERT(so->so_rcv.sb_mb == m);
		so->so_rcv.sb_mb = nextrecord;
		SB_EMPTY_FIXUP(&so->so_rcv);
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 2");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 2");

	while (m != NULL) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
#ifdef DIAGNOSTIC
		else if (m->m_type != MT_DATA && m->m_type != MT_HEADER)
			panic("receive 3");
#endif
		so->so_state &= ~SS_RCVATMARK;
		if (so->so_oobmark != 0 && so->so_oobmark < m->m_len)
			break;
		len = m->m_len;
		/*
		 * mp is set, just pass back the mbufs.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (m->m_flags & M_EOR)
			flags |= MSG_EOR;
		nextrecord = m->m_nextpkt;
		sbfree(&so->so_rcv, m);
		*mp = m;
		mp = &m->m_next;
		so->so_rcv.sb_mb = m = m->m_next;
		*mp = NULL;
		/*
		 * If m != NULL, we also know that
		 * so->so_rcv.sb_mb != NULL.
		 */
		KASSERT(so->so_rcv.sb_mb == m);
		if (m) {
			m->m_nextpkt = nextrecord;
			if (nextrecord == NULL)
				so->so_rcv.sb_lastrecord = m;
		} else {
			so->so_rcv.sb_mb = nextrecord;
			SB_EMPTY_FIXUP(&so->so_rcv);
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive 3");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive 3");
		if (so->so_oobmark) {
			so->so_oobmark -= len;
			if (so->so_oobmark == 0) {
				so->so_state |= SS_RCVATMARK;
				break;
			}
		}
		if (flags & MSG_EOR)
			break;
	}

	if (m != NULL) {
		m_freem(*mp);
		*mp = NULL;
		error = ENOMEM;
		(void) sbdroprecord(&so->so_rcv);
	} else {
		/*
		 * First part is an inline SB_EMPTY_FIXUP().  Second
		 * part makes sure sb_lastrecord is up-to-date if
		 * there is still data in the socket buffer.
		 */
		so->so_rcv.sb_mb = nextrecord;
		if (so->so_rcv.sb_mb == NULL) {
			so->so_rcv.sb_mbtail = NULL;
			so->so_rcv.sb_lastrecord = NULL;
		} else if (nextrecord->m_nextpkt == NULL)
			so->so_rcv.sb_lastrecord = nextrecord;
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 4");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 4");
	if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
		(*pr->pr_usrreqs->pr_rcvd)(so, flags, curlwp);
	if (*mp0 == NULL && (flags & MSG_EOR) == 0 &&
	    (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		goto restart;
	}

 release:
	sbunlock(&so->so_rcv);
	return error;
}

static struct socket *
gre_reconf(struct gre_softc *sc, const struct gre_soparm *newsoparm)
{
	struct ifnet *ifp = &sc->sc_if;

	GRE_DPRINTF(sc, "enter\n");

shutdown:
	if (sc->sc_soparm.sp_so != NULL) {
		GRE_DPRINTF(sc, "\n");
		gre_upcall_remove(sc->sc_soparm.sp_so);
		softint_disestablish(sc->sc_si);
		sc->sc_si = NULL;
		gre_fp_send(sc, GRE_M_DELFP, NULL);
		gre_clearconf(&sc->sc_soparm, false);
	}

	if (newsoparm != NULL) {
		GRE_DPRINTF(sc, "\n");
		sc->sc_soparm = *newsoparm;
		newsoparm = NULL;
	}

	if (sc->sc_soparm.sp_so != NULL) {
		GRE_DPRINTF(sc, "\n");
		sc->sc_si = softint_establish(SOFTINT_NET, greintr, sc);
		gre_upcall_add(sc->sc_soparm.sp_so, sc);
		if ((ifp->if_flags & IFF_UP) == 0) {
			GRE_DPRINTF(sc, "down\n");
			goto shutdown;
		}
	}

	GRE_DPRINTF(sc, "\n");
	if (sc->sc_soparm.sp_so != NULL)
		sc->sc_if.if_flags |= IFF_RUNNING;
	else {
		gre_bufq_purge(&sc->sc_snd);
		sc->sc_if.if_flags &= ~IFF_RUNNING;
	}
	return sc->sc_soparm.sp_so;
}

static int
gre_input(struct gre_softc *sc, struct mbuf *m, int hlen,
    const struct gre_h *gh)
{
	pktqueue_t *pktq = NULL;
	struct ifqueue *ifq = NULL;
	uint16_t flags;
	uint32_t af;		/* af passed to BPF tap */
	int isr = 0, s;

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	hlen += sizeof(struct gre_h);

	/* process GRE flags as packet can be of variable len */
	flags = ntohs(gh->flags);

	/* Checksum & Offset are present */
	if ((flags & GRE_CP) | (flags & GRE_RP))
		hlen += 4;
	/* We don't support routing fields (variable length) */
	if (flags & GRE_RP) {
		sc->sc_if.if_ierrors++;
		return 0;
	}
	if (flags & GRE_KP)
		hlen += 4;
	if (flags & GRE_SP)
		hlen += 4;

	switch (ntohs(gh->ptype)) { /* ethertypes */
#ifdef INET
	case ETHERTYPE_IP:
		pktq = ip_pktq;
		af = AF_INET;
		break;
#endif
#ifdef NETATALK
	case ETHERTYPE_ATALK:
		ifq = &atintrq1;
		isr = NETISR_ATALK;
		af = AF_APPLETALK;
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		pktq = ip6_pktq;
		af = AF_INET6;
		break;
#endif
#ifdef MPLS
	case ETHERTYPE_MPLS:
		ifq = &mplsintrq;
		isr = NETISR_MPLS;
		af = AF_MPLS;
		break;
#endif
	default:	   /* others not yet supported */
		GRE_DPRINTF(sc, "unhandled ethertype 0x%04x\n",
		    ntohs(gh->ptype));
		sc->sc_if.if_noproto++;
		return 0;
	}

	if (hlen > m->m_pkthdr.len) {
		m_freem(m);
		sc->sc_if.if_ierrors++;
		return EINVAL;
	}
	m_adj(m, hlen);

	bpf_mtap_af(&sc->sc_if, af, m);

	m->m_pkthdr.rcvif = &sc->sc_if;

	if (__predict_true(pktq)) {
		if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
			m_freem(m);
		}
		return 1;
	}

	s = splnet();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
	} else {
		IF_ENQUEUE(ifq, m);
	}
	/* we need schednetisr since the address family may change */
	schednetisr(isr);
	splx(s);

	return 1;	/* packet is done, no further processing needed */
}

/*
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->sc_soparm.sp_proto. See also RFC 1701 and RFC 2004
 */
static int
gre_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	   struct rtentry *rt)
{
	int error = 0;
	struct gre_softc *sc = ifp->if_softc;
	struct gre_h *gh;
	uint16_t etype = 0;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	bpf_mtap_af(ifp, dst->sa_family, m);

	m->m_flags &= ~(M_BCAST|M_MCAST);

	GRE_DPRINTF(sc, "dst->sa_family=%d\n", dst->sa_family);
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		/* TBD Extract the IP ToS field and set the
		 * encapsulating protocol's ToS to suit.
		 */
		etype = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
		etype = htons(ETHERTYPE_ATALK);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		etype = htons(ETHERTYPE_IPV6);
		break;
#endif
	default:
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = EAFNOSUPPORT;
		goto end;
	}

#ifdef MPLS
		if (rt != NULL && rt_gettag(rt) != NULL) {
			union mpls_shim msh;
			msh.s_addr = MPLS_GETSADDR(rt);
			if (msh.shim.label != MPLS_LABEL_IMPLNULL)
				etype = htons(ETHERTYPE_MPLS);
		}
#endif

	M_PREPEND(m, sizeof(*gh), M_DONTWAIT);

	if (m == NULL) {
		IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto end;
	}

	gh = mtod(m, struct gre_h *);
	gh->flags = 0;
	gh->ptype = etype;
	/* XXX Need to handle IP ToS.  Look at how I handle IP TTL. */

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/* Clear checksum-offload flags. */
	m->m_pkthdr.csum_flags = 0;
	m->m_pkthdr.csum_data = 0;

	/* send it off */
	if ((error = gre_bufq_enqueue(&sc->sc_snd, m)) != 0) {
		sc->sc_oflow_ev.ev_count++;
		m_freem(m);
	} else
		softint_schedule(sc->sc_si);
  end:
	if (error)
		ifp->if_oerrors++;
	return error;
}

static int
gre_getsockname(struct socket *so, struct sockaddr *nam)
{
	return (*so->so_proto->pr_usrreqs->pr_sockaddr)(so, nam);
}

static int
gre_getpeername(struct socket *so, struct sockaddr *nam)
{
	return (*so->so_proto->pr_usrreqs->pr_peeraddr)(so, nam);
}

static int
gre_getnames(struct socket *so, struct lwp *l, struct sockaddr_storage *src,
    struct sockaddr_storage *dst)
{
	struct sockaddr_storage ss;
	int rc;

	solock(so);
	if ((rc = gre_getsockname(so, (struct sockaddr *)&ss)) != 0)
		goto out;
	*src = ss;

	if ((rc = gre_getpeername(so, (struct sockaddr *)&ss)) != 0)
		goto out;
	*dst = ss;
out:
	sounlock(so);
	return rc;
}

static void
gre_fp_recvloop(void *arg)
{
	struct gre_softc *sc = arg;

	mutex_enter(&sc->sc_mtx);
	while (gre_fp_recv(sc))
		;
	mutex_exit(&sc->sc_mtx);
	kthread_exit(0);
}

static bool
gre_fp_recv(struct gre_softc *sc)
{
	int fd, ofd, rc;
	file_t *fp;

	fp = sc->sc_fp;
	ofd = sc->sc_fd;
	fd = -1;

	switch (sc->sc_msg) {
	case GRE_M_STOP:
		cv_signal(&sc->sc_fp_condvar);
		return false;
	case GRE_M_SETFP:
		mutex_exit(&sc->sc_mtx);
		rc = fd_dup(fp, 0, &fd, 0);
		mutex_enter(&sc->sc_mtx);
		if (rc != 0) {
			sc->sc_msg = GRE_M_ERR;
			break;
		}
		/*FALLTHROUGH*/
	case GRE_M_DELFP:
		mutex_exit(&sc->sc_mtx);
		if (ofd != -1 && fd_getfile(ofd) != NULL)
			fd_close(ofd);
		mutex_enter(&sc->sc_mtx);
		sc->sc_fd = fd;
		sc->sc_msg = GRE_M_OK;
		break;
	default:
		gre_fp_wait(sc);
		return true;
	}
	cv_signal(&sc->sc_fp_condvar);
	return true;
}

static bool
gre_fp_send(struct gre_softc *sc, enum gre_msg msg, file_t *fp)
{
	bool rc;

	mutex_enter(&sc->sc_mtx);
	while (sc->sc_msg != GRE_M_NONE)
		gre_fp_wait(sc);
	sc->sc_fp = fp;
	sc->sc_msg = msg;
	cv_signal(&sc->sc_fp_condvar);
	while (sc->sc_msg != GRE_M_STOP && sc->sc_msg != GRE_M_OK &&
	            sc->sc_msg != GRE_M_ERR)
		gre_fp_wait(sc);
	rc = (sc->sc_msg != GRE_M_ERR);
	sc->sc_msg = GRE_M_NONE;
	cv_signal(&sc->sc_fp_condvar);
	mutex_exit(&sc->sc_mtx);
	return rc;
}

static int
gre_ssock(struct ifnet *ifp, struct gre_soparm *sp, int fd)
{
	int error = 0;
	const struct protosw *pr;
	file_t *fp;
	struct gre_softc *sc = ifp->if_softc;
	struct socket *so;
	struct sockaddr_storage dst, src;

	if ((fp = fd_getfile(fd)) == NULL)
		return EBADF;
	if (fp->f_type != DTYPE_SOCKET) {
		fd_putfile(fd);
		return ENOTSOCK;
	}

	GRE_DPRINTF(sc, "\n");

	so = fp->f_socket;
	pr = so->so_proto;

	GRE_DPRINTF(sc, "type %d, proto %d\n", pr->pr_type, pr->pr_protocol);

	if ((pr->pr_flags & PR_ATOMIC) == 0 ||
	    (sp->sp_type != 0 && pr->pr_type != sp->sp_type) ||
	    (sp->sp_proto != 0 && pr->pr_protocol != 0 &&
	     pr->pr_protocol != sp->sp_proto)) {
		error = EINVAL;
		goto err;
	}

	GRE_DPRINTF(sc, "\n");

	/* check address */
	if ((error = gre_getnames(so, curlwp, &src, &dst)) != 0)
		goto err;

	GRE_DPRINTF(sc, "\n");

	if (!gre_fp_send(sc, GRE_M_SETFP, fp)) {
		error = EBUSY;
		goto err;
	}

	GRE_DPRINTF(sc, "\n");

	sp->sp_src = src;
	sp->sp_dst = dst;

	sp->sp_so = so;

err:
	fd_putfile(fd);
	return error;
}

static bool
sockaddr_is_anyaddr(const struct sockaddr *sa)
{
	socklen_t anylen, salen;
	const void *anyaddr, *addr;

	if ((anyaddr = sockaddr_anyaddr(sa, &anylen)) == NULL ||
	    (addr = sockaddr_const_addr(sa, &salen)) == NULL)
		return false;

	if (salen > anylen)
		return false;

	return memcmp(anyaddr, addr, MIN(anylen, salen)) == 0;
}

static bool
gre_is_nullconf(const struct gre_soparm *sp)
{
	return sockaddr_is_anyaddr(sstocsa(&sp->sp_src)) ||
	       sockaddr_is_anyaddr(sstocsa(&sp->sp_dst));
}

static void
gre_clearconf(struct gre_soparm *sp, bool force)
{
	if (sp->sp_bysock || force) {
		sockaddr_copy(sstosa(&sp->sp_src), sizeof(sp->sp_src),
		    sockaddr_any(sstosa(&sp->sp_src)));
		sockaddr_copy(sstosa(&sp->sp_dst), sizeof(sp->sp_dst),
		    sockaddr_any(sstosa(&sp->sp_dst)));
		sp->sp_bysock = false;
	}
	sp->sp_so = NULL; /* XXX */
}

static int
gre_ioctl(struct ifnet *ifp, const u_long cmd, void *data)
{
	struct ifreq *ifr;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct if_laddrreq *lifr = (struct if_laddrreq *)data;
	struct gre_softc *sc = ifp->if_softc;
	struct gre_soparm *sp;
	int fd, error = 0, oproto, otype, s;
	struct gre_soparm sp0;

	ifr = data;

	GRE_DPRINTF(sc, "cmd %lu\n", cmd);

	switch (cmd) {
	case GRESPROTO:
	case GRESADDRD:
	case GRESADDRS:
	case GRESSOCK:
	case GREDSOCK:
		if (kauth_authorize_network(curlwp->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, (void *)cmd,
		    NULL) != 0)
			return EPERM;
		break;
	default:
		break;
	}

	s = splnet();

	sp0 = sc->sc_soparm;
	sp0.sp_so = NULL;
	sp = &sp0;

	GRE_DPRINTF(sc, "\n");

	switch (cmd) {
	case SIOCINITIFADDR:
		GRE_DPRINTF(sc, "\n");
		if ((ifp->if_flags & IFF_UP) != 0)
			break;
		gre_clearconf(sp, false);
		ifp->if_flags |= IFF_UP;
		ifa->ifa_rtrequest = p2p_rtrequest;
		goto mksocket;
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		oproto = sp->sp_proto;
		otype = sp->sp_type;
		switch (ifr->ifr_flags & (IFF_LINK0|IFF_LINK2)) {
		case IFF_LINK0|IFF_LINK2:
			sp->sp_proto = IPPROTO_UDP;
			sp->sp_type = SOCK_DGRAM;
			break;
		case IFF_LINK2:
			sp->sp_proto = 0;
			sp->sp_type = 0;
			break;
		case IFF_LINK0:
			sp->sp_proto = IPPROTO_GRE;
			sp->sp_type = SOCK_RAW;
			break;
		default:
			GRE_DPRINTF(sc, "\n");
			error = EINVAL;
			goto out;
		}
		GRE_DPRINTF(sc, "\n");
		gre_clearconf(sp, false);
		if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) ==
		    (IFF_UP|IFF_RUNNING) &&
		    (oproto == sp->sp_proto || sp->sp_proto == 0) &&
		    (otype == sp->sp_type || sp->sp_type == 0))
			break;
		switch (sp->sp_proto) {
		case IPPROTO_UDP:
		case IPPROTO_GRE:
			goto mksocket;
		default:
			break;
		}
		break;
	case SIOCSIFMTU:
		/* XXX determine MTU automatically by probing w/
		 * XXX do-not-fragment packets?
		 */
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		/*FALLTHROUGH*/
	case SIOCGIFMTU:
		if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == NULL) {
			error = EAFNOSUPPORT;
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case GRESPROTO:
		gre_clearconf(sp, false);
		oproto = sp->sp_proto;
		otype = sp->sp_type;
		sp->sp_proto = ifr->ifr_flags;
		switch (sp->sp_proto) {
		case IPPROTO_UDP:
			ifp->if_flags |= IFF_LINK0|IFF_LINK2;
			sp->sp_type = SOCK_DGRAM;
			break;
		case IPPROTO_GRE:
			ifp->if_flags |= IFF_LINK0;
			ifp->if_flags &= ~IFF_LINK2;
			sp->sp_type = SOCK_RAW;
			break;
		case 0:
			ifp->if_flags &= ~IFF_LINK0;
			ifp->if_flags |= IFF_LINK2;
			sp->sp_type = 0;
			break;
		default:
			error = EPROTONOSUPPORT;
			break;
		}
		if ((oproto == sp->sp_proto || sp->sp_proto == 0) &&
		    (otype == sp->sp_type || sp->sp_type == 0))
			break;
		switch (sp->sp_proto) {
		case IPPROTO_UDP:
		case IPPROTO_GRE:
			goto mksocket;
		default:
			break;
		}
		break;
	case GREGPROTO:
		ifr->ifr_flags = sp->sp_proto;
		break;
	case GRESADDRS:
	case GRESADDRD:
		gre_clearconf(sp, false);
		/* set tunnel endpoints and mark interface as up */
		switch (cmd) {
		case GRESADDRS:
			sockaddr_copy(sstosa(&sp->sp_src),
			    sizeof(sp->sp_src), ifreq_getaddr(cmd, ifr));
			break;
		case GRESADDRD:
			sockaddr_copy(sstosa(&sp->sp_dst),
			    sizeof(sp->sp_dst), ifreq_getaddr(cmd, ifr));
			break;
		}
	checkaddr:
		if (sockaddr_any(sstosa(&sp->sp_src)) == NULL ||
		    sockaddr_any(sstosa(&sp->sp_dst)) == NULL) {
			error = EINVAL;
			break;
		}
		/* let gre_socreate() check the rest */
	mksocket:
		GRE_DPRINTF(sc, "\n");
		/* If we're administratively down, or the configuration
		 * is empty, there's no use creating a socket.
		 */
		if ((ifp->if_flags & IFF_UP) == 0 || gre_is_nullconf(sp))
			goto sendconf;

		GRE_DPRINTF(sc, "\n");
		fd = 0;
		error = gre_socreate(sc, sp, &fd);
		if (error != 0)
			break;

	setsock:
		GRE_DPRINTF(sc, "\n");

		error = gre_ssock(ifp, sp, fd);

		if (cmd != GRESSOCK) {
			GRE_DPRINTF(sc, "\n");
			/* XXX v. dodgy */
			if (fd_getfile(fd) != NULL)
				fd_close(fd);
		}

		if (error == 0) {
	sendconf:
			GRE_DPRINTF(sc, "\n");
			ifp->if_flags &= ~IFF_RUNNING;
			gre_reconf(sc, sp);
		}

		break;
	case GREGADDRS:
		ifreq_setaddr(cmd, ifr, sstosa(&sp->sp_src));
		break;
	case GREGADDRD:
		ifreq_setaddr(cmd, ifr, sstosa(&sp->sp_dst));
		break;
	case GREDSOCK:
		GRE_DPRINTF(sc, "\n");
		if (sp->sp_bysock)
			ifp->if_flags &= ~IFF_UP;
		gre_clearconf(sp, false);
		goto mksocket;
	case GRESSOCK:
		GRE_DPRINTF(sc, "\n");
		gre_clearconf(sp, true);
		fd = (int)ifr->ifr_value;
		sp->sp_bysock = true;
		ifp->if_flags |= IFF_UP;
		goto setsock;
	case SIOCSLIFPHYADDR:
		GRE_DPRINTF(sc, "\n");
		if (lifr->addr.ss_family != lifr->dstaddr.ss_family) {
			error = EAFNOSUPPORT;
			break;
		}
		sockaddr_copy(sstosa(&sp->sp_src), sizeof(sp->sp_src),
		    sstosa(&lifr->addr));
		sockaddr_copy(sstosa(&sp->sp_dst), sizeof(sp->sp_dst),
		    sstosa(&lifr->dstaddr));
		GRE_DPRINTF(sc, "\n");
		goto checkaddr;
	case SIOCDIFPHYADDR:
		GRE_DPRINTF(sc, "\n");
		gre_clearconf(sp, true);
		ifp->if_flags &= ~IFF_UP;
		goto mksocket;
	case SIOCGLIFPHYADDR:
		GRE_DPRINTF(sc, "\n");
		if (gre_is_nullconf(sp)) {
			error = EADDRNOTAVAIL;
			break;
		}
		sockaddr_copy(sstosa(&lifr->addr), sizeof(lifr->addr),
		    sstosa(&sp->sp_src));
		sockaddr_copy(sstosa(&lifr->dstaddr), sizeof(lifr->dstaddr),
		    sstosa(&sp->sp_dst));
		GRE_DPRINTF(sc, "\n");
		break;
	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}
out:
	GRE_DPRINTF(sc, "\n");
	splx(s);
	return error;
}

/* ARGSUSED */
void
greattach(int count)
{
	if_clone_attach(&gre_cloner);
}
