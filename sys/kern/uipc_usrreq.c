/*	$NetBSD: uipc_usrreq.c,v 1.179 2015/05/02 17:18:03 rtr Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2004, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Andrew Doran.
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
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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
 *	@(#)uipc_usrreq.c	8.9 (Berkeley) 5/14/95
 */

/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)uipc_usrreq.c	8.9 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_usrreq.c,v 1.179 2015/05/02 17:18:03 rtr Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/unpcb.h>
#include <sys/un.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mbuf.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/atomic.h>
#include <sys/uidinfo.h>
#include <sys/kernel.h>
#include <sys/kthread.h>

/*
 * Unix communications domain.
 *
 * TODO:
 *	RDM
 *	rethink name space problems
 *	need a proper out-of-band
 *
 * Notes on locking:
 *
 * The generic rules noted in uipc_socket2.c apply.  In addition:
 *
 * o We have a global lock, uipc_lock.
 *
 * o All datagram sockets are locked by uipc_lock.
 *
 * o For stream socketpairs, the two endpoints are created sharing the same
 *   independent lock.  Sockets presented to PRU_CONNECT2 must already have
 *   matching locks.
 *
 * o Stream sockets created via socket() start life with their own
 *   independent lock.
 * 
 * o Stream connections to a named endpoint are slightly more complicated.
 *   Sockets that have called listen() have their lock pointer mutated to
 *   the global uipc_lock.  When establishing a connection, the connecting
 *   socket also has its lock mutated to uipc_lock, which matches the head
 *   (listening socket).  We create a new socket for accept() to return, and
 *   that also shares the head's lock.  Until the connection is completely
 *   done on both ends, all three sockets are locked by uipc_lock.  Once the
 *   connection is complete, the association with the head's lock is broken.
 *   The connecting socket and the socket returned from accept() have their
 *   lock pointers mutated away from uipc_lock, and back to the connecting
 *   socket's original, independent lock.  The head continues to be locked
 *   by uipc_lock.
 *
 * o If uipc_lock is determined to be a significant source of contention,
 *   it could easily be hashed out.  It is difficult to simply make it an
 *   independent lock because of visibility / garbage collection issues:
 *   if a socket has been associated with a lock at any point, that lock
 *   must remain valid until the socket is no longer visible in the system.
 *   The lock must not be freed or otherwise destroyed until any sockets
 *   that had referenced it have also been destroyed.
 */
const struct sockaddr_un sun_noname = {
	.sun_len = offsetof(struct sockaddr_un, sun_path),
	.sun_family = AF_LOCAL,
};
ino_t	unp_ino;			/* prototype for fake inode numbers */

static struct mbuf * unp_addsockcred(struct lwp *, struct mbuf *);
static void   unp_discard_later(file_t *);
static void   unp_discard_now(file_t *);
static void   unp_disconnect1(struct unpcb *);
static bool   unp_drop(struct unpcb *, int);
static int    unp_internalize(struct mbuf **);
static void   unp_mark(file_t *);
static void   unp_scan(struct mbuf *, void (*)(file_t *), int);
static void   unp_shutdown1(struct unpcb *);
static void   unp_thread(void *);
static void   unp_thread_kick(void);

static kmutex_t *uipc_lock;

static kcondvar_t unp_thread_cv;
static lwp_t *unp_thread_lwp;
static SLIST_HEAD(,file) unp_thread_discard;
static int unp_defer;

/*
 * Initialize Unix protocols.
 */
void
uipc_init(void)
{
	int error;

	uipc_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
	cv_init(&unp_thread_cv, "unpgc");

	error = kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL, unp_thread,
	    NULL, &unp_thread_lwp, "unpgc");
	if (error != 0)
		panic("uipc_init %d", error);
}

/*
 * A connection succeeded: disassociate both endpoints from the head's
 * lock, and make them share their own lock.  There is a race here: for
 * a very brief time one endpoint will be locked by a different lock
 * than the other end.  However, since the current thread holds the old
 * lock (the listening socket's lock, the head) access can still only be
 * made to one side of the connection.
 */
static void
unp_setpeerlocks(struct socket *so, struct socket *so2)
{
	struct unpcb *unp;
	kmutex_t *lock;

	KASSERT(solocked2(so, so2));

	/*
	 * Bail out if either end of the socket is not yet fully
	 * connected or accepted.  We only break the lock association
	 * with the head when the pair of sockets stand completely
	 * on their own.
	 */
	KASSERT(so->so_head == NULL);
	if (so2->so_head != NULL)
		return;

	/*
	 * Drop references to old lock.  A third reference (from the
	 * queue head) must be held as we still hold its lock.  Bonus:
	 * we don't need to worry about garbage collecting the lock.
	 */
	lock = so->so_lock;
	KASSERT(lock == uipc_lock);
	mutex_obj_free(lock);
	mutex_obj_free(lock);

	/*
	 * Grab stream lock from the initiator and share between the two
	 * endpoints.  Issue memory barrier to ensure all modifications
	 * become globally visible before the lock change.  so2 is
	 * assumed not to have a stream lock, because it was created
	 * purely for the server side to accept this connection and
	 * started out life using the domain-wide lock.
	 */
	unp = sotounpcb(so);
	KASSERT(unp->unp_streamlock != NULL);
	KASSERT(sotounpcb(so2)->unp_streamlock == NULL);
	lock = unp->unp_streamlock;
	unp->unp_streamlock = NULL;
	mutex_obj_hold(lock);
	membar_exit();
	/*
	 * possible race if lock is not held - see comment in
	 * uipc_usrreq(PRU_ACCEPT).
	 */
	KASSERT(mutex_owned(lock));
	solockreset(so, lock);
	solockreset(so2, lock);
}

/*
 * Reset a socket's lock back to the domain-wide lock.
 */
static void
unp_resetlock(struct socket *so)
{
	kmutex_t *olock, *nlock;
	struct unpcb *unp;

	KASSERT(solocked(so));

	olock = so->so_lock;
	nlock = uipc_lock;
	if (olock == nlock)
		return;
	unp = sotounpcb(so);
	KASSERT(unp->unp_streamlock == NULL);
	unp->unp_streamlock = olock;
	mutex_obj_hold(nlock);
	mutex_enter(nlock);
	solockreset(so, nlock);
	mutex_exit(olock);
}

static void
unp_free(struct unpcb *unp)
{
	if (unp->unp_addr)
		free(unp->unp_addr, M_SONAME);
	if (unp->unp_streamlock != NULL)
		mutex_obj_free(unp->unp_streamlock);
	kmem_free(unp, sizeof(*unp));
}

static int
unp_output(struct mbuf *m, struct mbuf *control, struct unpcb *unp)
{
	struct socket *so2;
	const struct sockaddr_un *sun;

	/* XXX: server side closed the socket */
	if (unp->unp_conn == NULL)
		return ECONNREFUSED;
	so2 = unp->unp_conn->unp_socket;

	KASSERT(solocked(so2));

	if (unp->unp_addr)
		sun = unp->unp_addr;
	else
		sun = &sun_noname;
	if (unp->unp_conn->unp_flags & UNP_WANTCRED)
		control = unp_addsockcred(curlwp, control);
	if (sbappendaddr(&so2->so_rcv, (const struct sockaddr *)sun, m,
	    control) == 0) {
		so2->so_rcv.sb_overflowed++;
		unp_dispose(control);
		m_freem(control);
		m_freem(m);
		return (ENOBUFS);
	} else {
		sorwakeup(so2);
		return (0);
	}
}

static void
unp_setaddr(struct socket *so, struct sockaddr *nam, bool peeraddr)
{
	const struct sockaddr_un *sun = NULL;
	struct unpcb *unp;

	KASSERT(solocked(so));
	unp = sotounpcb(so);

	if (peeraddr) {
		if (unp->unp_conn && unp->unp_conn->unp_addr)
			sun = unp->unp_conn->unp_addr;
	} else {
		if (unp->unp_addr)
			sun = unp->unp_addr;
	}
	if (sun == NULL)
		sun = &sun_noname;

	memcpy(nam, sun, sun->sun_len);
}

static int
unp_rcvd(struct socket *so, int flags, struct lwp *l)
{
	struct unpcb *unp = sotounpcb(so);
	struct socket *so2;
	u_int newhiwat;

	KASSERT(solocked(so));
	KASSERT(unp != NULL);

	switch (so->so_type) {

	case SOCK_DGRAM:
		panic("uipc 1");
		/*NOTREACHED*/

	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:
#define	rcv (&so->so_rcv)
#define snd (&so2->so_snd)
		if (unp->unp_conn == 0)
			break;
		so2 = unp->unp_conn->unp_socket;
		KASSERT(solocked2(so, so2));
		/*
		 * Adjust backpressure on sender
		 * and wakeup any waiting to write.
		 */
		snd->sb_mbmax += unp->unp_mbcnt - rcv->sb_mbcnt;
		unp->unp_mbcnt = rcv->sb_mbcnt;
		newhiwat = snd->sb_hiwat + unp->unp_cc - rcv->sb_cc;
		(void)chgsbsize(so2->so_uidinfo,
		    &snd->sb_hiwat, newhiwat, RLIM_INFINITY);
		unp->unp_cc = rcv->sb_cc;
		sowwakeup(so2);
#undef snd
#undef rcv
		break;

	default:
		panic("uipc 2");
	}

	return 0;
}

static int
unp_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
unp_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	struct unpcb *unp = sotounpcb(so);
	int error = 0;
	u_int newhiwat;
	struct socket *so2;

	KASSERT(solocked(so));
	KASSERT(unp != NULL);
	KASSERT(m != NULL);

	/*
	 * Note: unp_internalize() rejects any control message
	 * other than SCM_RIGHTS, and only allows one.  This
	 * has the side-effect of preventing a caller from
	 * forging SCM_CREDS.
	 */
	if (control) {
		sounlock(so);
		error = unp_internalize(&control);
		solock(so);
		if (error != 0) {
			m_freem(control);
			m_freem(m);
			return error;
		}
	}

	switch (so->so_type) {

	case SOCK_DGRAM: {
		KASSERT(so->so_lock == uipc_lock);
		if (nam) {
			if ((so->so_state & SS_ISCONNECTED) != 0)
				error = EISCONN;
			else {
				/*
				 * Note: once connected, the
				 * socket's lock must not be
				 * dropped until we have sent
				 * the message and disconnected.
				 * This is necessary to prevent
				 * intervening control ops, like
				 * another connection.
				 */
				error = unp_connect(so, nam, l);
			}
		} else {
			if ((so->so_state & SS_ISCONNECTED) == 0)
				error = ENOTCONN;
		}
		if (error) {
			unp_dispose(control);
			m_freem(control);
			m_freem(m);
			return error;
		}
		error = unp_output(m, control, unp);
		if (nam)
			unp_disconnect1(unp);
		break;
	}

	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:
#define	rcv (&so2->so_rcv)
#define	snd (&so->so_snd)
		if (unp->unp_conn == NULL) {
			error = ENOTCONN;
			break;
		}
		so2 = unp->unp_conn->unp_socket;
		KASSERT(solocked2(so, so2));
		if (unp->unp_conn->unp_flags & UNP_WANTCRED) {
			/*
			 * Credentials are passed only once on
			 * SOCK_STREAM and SOCK_SEQPACKET.
			 */
			unp->unp_conn->unp_flags &= ~UNP_WANTCRED;
			control = unp_addsockcred(l, control);
		}
		/*
		 * Send to paired receive port, and then reduce
		 * send buffer hiwater marks to maintain backpressure.
		 * Wake up readers.
		 */
		if (control) {
			if (sbappendcontrol(rcv, m, control) != 0)
				control = NULL;
		} else {
			switch(so->so_type) {
			case SOCK_SEQPACKET:
				sbappendrecord(rcv, m);
				break;
			case SOCK_STREAM:
				sbappend(rcv, m);
				break;
			default:
				panic("uipc_usrreq");
				break;
			}
		}
		snd->sb_mbmax -=
		    rcv->sb_mbcnt - unp->unp_conn->unp_mbcnt;
		unp->unp_conn->unp_mbcnt = rcv->sb_mbcnt;
		newhiwat = snd->sb_hiwat -
		    (rcv->sb_cc - unp->unp_conn->unp_cc);
		(void)chgsbsize(so->so_uidinfo,
		    &snd->sb_hiwat, newhiwat, RLIM_INFINITY);
		unp->unp_conn->unp_cc = rcv->sb_cc;
		sorwakeup(so2);
#undef snd
#undef rcv
		if (control != NULL) {
			unp_dispose(control);
			m_freem(control);
		}
		break;

	default:
		panic("uipc 4");
	}

	return error;
}

static int
unp_sendoob(struct socket *so, struct mbuf *m, struct mbuf * control)
{
	KASSERT(solocked(so));

	m_freem(m);
	m_freem(control);

	return EOPNOTSUPP;
}

/*
 * Unix domain socket option processing.
 */
int
uipc_ctloutput(int op, struct socket *so, struct sockopt *sopt)
{
	struct unpcb *unp = sotounpcb(so);
	int optval = 0, error = 0;

	KASSERT(solocked(so));

	if (sopt->sopt_level != 0) {
		error = ENOPROTOOPT;
	} else switch (op) {

	case PRCO_SETOPT:
		switch (sopt->sopt_name) {
		case LOCAL_CREDS:
		case LOCAL_CONNWAIT:
			error = sockopt_getint(sopt, &optval);
			if (error)
				break;
			switch (sopt->sopt_name) {
#define	OPTSET(bit) \
	if (optval) \
		unp->unp_flags |= (bit); \
	else \
		unp->unp_flags &= ~(bit);

			case LOCAL_CREDS:
				OPTSET(UNP_WANTCRED);
				break;
			case LOCAL_CONNWAIT:
				OPTSET(UNP_CONNWAIT);
				break;
			}
			break;
#undef OPTSET

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case PRCO_GETOPT:
		sounlock(so);
		switch (sopt->sopt_name) {
		case LOCAL_PEEREID:
			if (unp->unp_flags & UNP_EIDSVALID) {
				error = sockopt_set(sopt,
				    &unp->unp_connid, sizeof(unp->unp_connid));
			} else {
				error = EINVAL;
			}
			break;
		case LOCAL_CREDS:
#define	OPTBIT(bit)	(unp->unp_flags & (bit) ? 1 : 0)

			optval = OPTBIT(UNP_WANTCRED);
			error = sockopt_setint(sopt, optval);
			break;
#undef OPTBIT

		default:
			error = ENOPROTOOPT;
			break;
		}
		solock(so);
		break;
	}
	return (error);
}

/*
 * Both send and receive buffers are allocated PIPSIZ bytes of buffering
 * for stream sockets, although the total for sender and receiver is
 * actually only PIPSIZ.
 * Datagram sockets really use the sendspace as the maximum datagram size,
 * and don't really want to reserve the sendspace.  Their recvspace should
 * be large enough for at least one max-size datagram plus address.
 */
#define	PIPSIZ	4096
u_long	unpst_sendspace = PIPSIZ;
u_long	unpst_recvspace = PIPSIZ;
u_long	unpdg_sendspace = 2*1024;	/* really max datagram size */
u_long	unpdg_recvspace = 4*1024;

u_int	unp_rights;			/* files in flight */
u_int	unp_rights_ratio = 2;		/* limit, fraction of maxfiles */

static int
unp_attach(struct socket *so, int proto)
{
	struct unpcb *unp = sotounpcb(so);
	u_long sndspc, rcvspc;
	int error;

	KASSERT(unp == NULL);

	switch (so->so_type) {
	case SOCK_SEQPACKET:
		/* FALLTHROUGH */
	case SOCK_STREAM:
		if (so->so_lock == NULL) {
			so->so_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
			solock(so);
		}
		sndspc = unpst_sendspace;
		rcvspc = unpst_recvspace;
		break;

	case SOCK_DGRAM:
		if (so->so_lock == NULL) {
			mutex_obj_hold(uipc_lock);
			so->so_lock = uipc_lock;
			solock(so);
		}
		sndspc = unpdg_sendspace;
		rcvspc = unpdg_recvspace;
		break;

	default:
		panic("unp_attach");
	}

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, sndspc, rcvspc);
		if (error) {
			return error;
		}
	}

	unp = kmem_zalloc(sizeof(*unp), KM_SLEEP);
	nanotime(&unp->unp_ctime);
	unp->unp_socket = so;
	so->so_pcb = unp;

	KASSERT(solocked(so));
	return 0;
}

static void
unp_detach(struct socket *so)
{
	struct unpcb *unp;
	vnode_t *vp;

	unp = sotounpcb(so);
	KASSERT(unp != NULL);
	KASSERT(solocked(so));
 retry:
	if ((vp = unp->unp_vnode) != NULL) {
		sounlock(so);
		/* Acquire v_interlock to protect against unp_connect(). */
		/* XXXAD racy */
		mutex_enter(vp->v_interlock);
		vp->v_socket = NULL;
		mutex_exit(vp->v_interlock);
		vrele(vp);
		solock(so);
		unp->unp_vnode = NULL;
	}
	if (unp->unp_conn)
		unp_disconnect1(unp);
	while (unp->unp_refs) {
		KASSERT(solocked2(so, unp->unp_refs->unp_socket));
		if (unp_drop(unp->unp_refs, ECONNRESET)) {
			solock(so);
			goto retry;
		}
	}
	soisdisconnected(so);
	so->so_pcb = NULL;
	if (unp_rights) {
		/*
		 * Normally the receive buffer is flushed later, in sofree,
		 * but if our receive buffer holds references to files that
		 * are now garbage, we will enqueue those file references to
		 * the garbage collector and kick it into action.
		 */
		sorflush(so);
		unp_free(unp);
		unp_thread_kick();
	} else
		unp_free(unp);
}

static int
unp_accept(struct socket *so, struct sockaddr *nam)
{
	struct unpcb *unp = sotounpcb(so);
	struct socket *so2;

	KASSERT(solocked(so));
	KASSERT(nam != NULL);

	/* XXX code review required to determine if unp can ever be NULL */
	if (unp == NULL)
		return EINVAL;

	KASSERT(so->so_lock == uipc_lock);
	/*
	 * Mark the initiating STREAM socket as connected *ONLY*
	 * after it's been accepted.  This prevents a client from
	 * overrunning a server and receiving ECONNREFUSED.
	 */
	if (unp->unp_conn == NULL) {
		/*
		 * This will use the empty socket and will not
		 * allocate.
		 */
		unp_setaddr(so, nam, true);
		return 0;
	}
	so2 = unp->unp_conn->unp_socket;
	if (so2->so_state & SS_ISCONNECTING) {
		KASSERT(solocked2(so, so->so_head));
		KASSERT(solocked2(so2, so->so_head));
		soisconnected(so2);
	}
	/*
	 * If the connection is fully established, break the
	 * association with uipc_lock and give the connected
	 * pair a separate lock to share.
	 * There is a race here: sotounpcb(so2)->unp_streamlock
	 * is not locked, so when changing so2->so_lock
	 * another thread can grab it while so->so_lock is still
	 * pointing to the (locked) uipc_lock.
	 * this should be harmless, except that this makes
	 * solocked2() and solocked() unreliable.
	 * Another problem is that unp_setaddr() expects the
	 * the socket locked. Grabing sotounpcb(so2)->unp_streamlock
	 * fixes both issues.
	 */
	mutex_enter(sotounpcb(so2)->unp_streamlock);
	unp_setpeerlocks(so2, so);
	/*
	 * Only now return peer's address, as we may need to
	 * block in order to allocate memory.
	 *
	 * XXX Minor race: connection can be broken while
	 * lock is dropped in unp_setaddr().  We will return
	 * error == 0 and sun_noname as the peer address.
	 */
	unp_setaddr(so, nam, true);
	/* so_lock now points to unp_streamlock */
	mutex_exit(so2->so_lock);
	return 0;
}

static int
unp_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	return EOPNOTSUPP;
}

static int
unp_stat(struct socket *so, struct stat *ub)
{
	struct unpcb *unp;
	struct socket *so2;

	KASSERT(solocked(so));

	unp = sotounpcb(so);
	if (unp == NULL)
		return EINVAL;

	ub->st_blksize = so->so_snd.sb_hiwat;
	switch (so->so_type) {
	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:
		if (unp->unp_conn == 0) 
			break;

		so2 = unp->unp_conn->unp_socket;
		KASSERT(solocked2(so, so2));
		ub->st_blksize += so2->so_rcv.sb_cc;
		break;
	default:
		break;
	}
	ub->st_dev = NODEV;
	if (unp->unp_ino == 0)
		unp->unp_ino = unp_ino++;
	ub->st_atimespec = ub->st_mtimespec = ub->st_ctimespec = unp->unp_ctime;
	ub->st_ino = unp->unp_ino;
	return (0);
}

static int
unp_peeraddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));
	KASSERT(sotounpcb(so) != NULL);
	KASSERT(nam != NULL);

	unp_setaddr(so, nam, true);
	return 0;
}

static int
unp_sockaddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));
	KASSERT(sotounpcb(so) != NULL);
	KASSERT(nam != NULL);

	unp_setaddr(so, nam, false);
	return 0;
}

/*
 * we only need to perform this allocation until syscalls other than
 * bind are adjusted to use sockaddr_big.
 */
static struct sockaddr_un *
makeun_sb(struct sockaddr *nam, size_t *addrlen)
{
	struct sockaddr_un *sun;

	*addrlen = nam->sa_len + 1;
	sun = malloc(*addrlen, M_SONAME, M_WAITOK);
	memcpy(sun, nam, nam->sa_len);
	*(((char *)sun) + nam->sa_len) = '\0';
	return sun;
}

static int
unp_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct sockaddr_un *sun;
	struct unpcb *unp;
	vnode_t *vp;
	struct vattr vattr;
	size_t addrlen;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;
	proc_t *p;

	unp = sotounpcb(so);

	KASSERT(solocked(so));
	KASSERT(unp != NULL);
	KASSERT(nam != NULL);

	if (unp->unp_vnode != NULL)
		return (EINVAL);
	if ((unp->unp_flags & UNP_BUSY) != 0) {
		/*
		 * EALREADY may not be strictly accurate, but since this
		 * is a major application error it's hardly a big deal.
		 */
		return (EALREADY);
	}
	unp->unp_flags |= UNP_BUSY;
	sounlock(so);

	p = l->l_proc;
	sun = makeun_sb(nam, &addrlen);

	pb = pathbuf_create(sun->sun_path);
	if (pb == NULL) {
		error = ENOMEM;
		goto bad;
	}
	NDINIT(&nd, CREATE, FOLLOW | LOCKPARENT | TRYEMULROOT, pb);

/* SHOULD BE ABLE TO ADOPT EXISTING AND wakeup() ALA FIFO's */
	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		goto bad;
	}
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		pathbuf_destroy(pb);
		error = EADDRINUSE;
		goto bad;
	}
	vattr_null(&vattr);
	vattr.va_type = VSOCK;
	vattr.va_mode = ACCESSPERMS & ~(p->p_cwdi->cwdi_cmask);
	error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (error) {
		vput(nd.ni_dvp);
		pathbuf_destroy(pb);
		goto bad;
	}
	vp = nd.ni_vp;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	solock(so);
	vp->v_socket = unp->unp_socket;
	unp->unp_vnode = vp;
	unp->unp_addrlen = addrlen;
	unp->unp_addr = sun;
	unp->unp_connid.unp_pid = p->p_pid;
	unp->unp_connid.unp_euid = kauth_cred_geteuid(l->l_cred);
	unp->unp_connid.unp_egid = kauth_cred_getegid(l->l_cred);
	unp->unp_flags |= UNP_EIDSBIND;
	VOP_UNLOCK(vp);
	vput(nd.ni_dvp);
	unp->unp_flags &= ~UNP_BUSY;
	pathbuf_destroy(pb);
	return (0);

 bad:
	free(sun, M_SONAME);
	solock(so);
	unp->unp_flags &= ~UNP_BUSY;
	return (error);
}

static int
unp_listen(struct socket *so, struct lwp *l)
{
	struct unpcb *unp = sotounpcb(so);

	KASSERT(solocked(so));
	KASSERT(unp != NULL);

	/*
	 * If the socket can accept a connection, it must be
	 * locked by uipc_lock.
	 */
	unp_resetlock(so);
	if (unp->unp_vnode == NULL)
		return EINVAL;

	return 0;
}

static int
unp_disconnect(struct socket *so)
{
	KASSERT(solocked(so));
	KASSERT(sotounpcb(so) != NULL);

	unp_disconnect1(sotounpcb(so));
	return 0;
}

static int
unp_shutdown(struct socket *so)
{
	KASSERT(solocked(so));
	KASSERT(sotounpcb(so) != NULL);

	socantsendmore(so);
	unp_shutdown1(sotounpcb(so));
	return 0;
}

static int
unp_abort(struct socket *so)
{
	KASSERT(solocked(so));
	KASSERT(sotounpcb(so) != NULL);

	(void)unp_drop(sotounpcb(so), ECONNABORTED);
	KASSERT(so->so_head == NULL);
	KASSERT(so->so_pcb != NULL);
	unp_detach(so);
	return 0;
}

static int
unp_connect1(struct socket *so, struct socket *so2, struct lwp *l)
{
	struct unpcb *unp = sotounpcb(so);
	struct unpcb *unp2;

	if (so2->so_type != so->so_type)
		return EPROTOTYPE;

	/*
	 * All three sockets involved must be locked by same lock:
	 *
	 * local endpoint (so)
	 * remote endpoint (so2)
	 * queue head (so2->so_head, only if PR_CONNREQUIRED)
	 */
	KASSERT(solocked2(so, so2));
	KASSERT(so->so_head == NULL);
	if (so2->so_head != NULL) {
		KASSERT(so2->so_lock == uipc_lock);
		KASSERT(solocked2(so2, so2->so_head));
	}

	unp2 = sotounpcb(so2);
	unp->unp_conn = unp2;

	if ((so->so_proto->pr_flags & PR_CONNREQUIRED) != 0) {
		unp2->unp_connid.unp_pid = l->l_proc->p_pid;
		unp2->unp_connid.unp_euid = kauth_cred_geteuid(l->l_cred);
		unp2->unp_connid.unp_egid = kauth_cred_getegid(l->l_cred);
		unp2->unp_flags |= UNP_EIDSVALID;
		if (unp2->unp_flags & UNP_EIDSBIND) {
			unp->unp_connid = unp2->unp_connid;
			unp->unp_flags |= UNP_EIDSVALID;
		}
	}

	switch (so->so_type) {

	case SOCK_DGRAM:
		unp->unp_nextref = unp2->unp_refs;
		unp2->unp_refs = unp;
		soisconnected(so);
		break;

	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:

		/*
		 * SOCK_SEQPACKET and SOCK_STREAM cases are handled by callers
		 * which are unp_connect() or unp_connect2().
		 */

		break;

	default:
		panic("unp_connect1");
	}

	return 0;
}

int
unp_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct sockaddr_un *sun;
	vnode_t *vp;
	struct socket *so2, *so3;
	struct unpcb *unp, *unp2, *unp3;
	size_t addrlen;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	unp = sotounpcb(so);
	if ((unp->unp_flags & UNP_BUSY) != 0) {
		/*
		 * EALREADY may not be strictly accurate, but since this
		 * is a major application error it's hardly a big deal.
		 */
		return (EALREADY);
	}
	unp->unp_flags |= UNP_BUSY;
	sounlock(so);

	sun = makeun_sb(nam, &addrlen);
	pb = pathbuf_create(sun->sun_path);
	if (pb == NULL) {
		error = ENOMEM;
		goto bad2;
	}

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, pb);

	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		goto bad2;
	}
	vp = nd.ni_vp;
	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto bad;
	}
	pathbuf_destroy(pb);
	if ((error = VOP_ACCESS(vp, VWRITE, l->l_cred)) != 0)
		goto bad;
	/* Acquire v_interlock to protect against unp_detach(). */
	mutex_enter(vp->v_interlock);
	so2 = vp->v_socket;
	if (so2 == NULL) {
		mutex_exit(vp->v_interlock);
		error = ECONNREFUSED;
		goto bad;
	}
	if (so->so_type != so2->so_type) {
		mutex_exit(vp->v_interlock);
		error = EPROTOTYPE;
		goto bad;
	}
	solock(so);
	unp_resetlock(so);
	mutex_exit(vp->v_interlock);
	if ((so->so_proto->pr_flags & PR_CONNREQUIRED) != 0) {
		/*
		 * This may seem somewhat fragile but is OK: if we can
		 * see SO_ACCEPTCONN set on the endpoint, then it must
		 * be locked by the domain-wide uipc_lock.
		 */
		KASSERT((so2->so_options & SO_ACCEPTCONN) == 0 ||
		    so2->so_lock == uipc_lock);
		if ((so2->so_options & SO_ACCEPTCONN) == 0 ||
		    (so3 = sonewconn(so2, false)) == NULL) {
			error = ECONNREFUSED;
			sounlock(so);
			goto bad;
		}
		unp2 = sotounpcb(so2);
		unp3 = sotounpcb(so3);
		if (unp2->unp_addr) {
			unp3->unp_addr = malloc(unp2->unp_addrlen,
			    M_SONAME, M_WAITOK);
			memcpy(unp3->unp_addr, unp2->unp_addr,
			    unp2->unp_addrlen);
			unp3->unp_addrlen = unp2->unp_addrlen;
		}
		unp3->unp_flags = unp2->unp_flags;
		so2 = so3;
	}
	error = unp_connect1(so, so2, l);
	if (error) {
		sounlock(so);
		goto bad;
	}
	unp2 = sotounpcb(so2);
	switch (so->so_type) {

	/*
	 * SOCK_DGRAM and default cases are handled in prior call to
	 * unp_connect1(), do not add a default case without fixing
	 * unp_connect1().
	 */

	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:
		unp2->unp_conn = unp;
		if ((unp->unp_flags | unp2->unp_flags) & UNP_CONNWAIT)
			soisconnecting(so);
		else
			soisconnected(so);
		soisconnected(so2);
		/*
		 * If the connection is fully established, break the
		 * association with uipc_lock and give the connected
		 * pair a seperate lock to share.
		 */
		KASSERT(so2->so_head != NULL);
		unp_setpeerlocks(so, so2);
		break;

	}
	sounlock(so);
 bad:
	vput(vp);
 bad2:
	free(sun, M_SONAME);
	solock(so);
	unp->unp_flags &= ~UNP_BUSY;
	return (error);
}

int
unp_connect2(struct socket *so, struct socket *so2)
{
	struct unpcb *unp = sotounpcb(so);
	struct unpcb *unp2;
	int error = 0;

	KASSERT(solocked2(so, so2));

	error = unp_connect1(so, so2, curlwp);
	if (error)
		return error;

	unp2 = sotounpcb(so2);
	switch (so->so_type) {

	/*
	 * SOCK_DGRAM and default cases are handled in prior call to
	 * unp_connect1(), do not add a default case without fixing
	 * unp_connect1().
	 */

	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:
		unp2->unp_conn = unp;
		if ((so->so_proto->pr_flags & PR_CONNREQUIRED) != 0) {
			unp->unp_connid = unp2->unp_connid;
			unp->unp_flags |= UNP_EIDSVALID;
		}
		soisconnected(so);
		soisconnected(so2);
		break;

	}
	return error;
}

static void
unp_disconnect1(struct unpcb *unp)
{
	struct unpcb *unp2 = unp->unp_conn;
	struct socket *so;

	if (unp2 == 0)
		return;
	unp->unp_conn = 0;
	so = unp->unp_socket;
	switch (so->so_type) {
	case SOCK_DGRAM:
		if (unp2->unp_refs == unp)
			unp2->unp_refs = unp->unp_nextref;
		else {
			unp2 = unp2->unp_refs;
			for (;;) {
				KASSERT(solocked2(so, unp2->unp_socket));
				if (unp2 == 0)
					panic("unp_disconnect1");
				if (unp2->unp_nextref == unp)
					break;
				unp2 = unp2->unp_nextref;
			}
			unp2->unp_nextref = unp->unp_nextref;
		}
		unp->unp_nextref = 0;
		so->so_state &= ~SS_ISCONNECTED;
		break;

	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:
		KASSERT(solocked2(so, unp2->unp_socket));
		soisdisconnected(so);
		unp2->unp_conn = 0;
		soisdisconnected(unp2->unp_socket);
		break;
	}
}

static void
unp_shutdown1(struct unpcb *unp)
{
	struct socket *so;

	switch(unp->unp_socket->so_type) {
	case SOCK_SEQPACKET: /* FALLTHROUGH */
	case SOCK_STREAM:
		if (unp->unp_conn && (so = unp->unp_conn->unp_socket))
			socantrcvmore(so);
		break;
	default:
		break;
	}
}

static bool
unp_drop(struct unpcb *unp, int errno)
{
	struct socket *so = unp->unp_socket;

	KASSERT(solocked(so));

	so->so_error = errno;
	unp_disconnect1(unp);
	if (so->so_head) {
		so->so_pcb = NULL;
		/* sofree() drops the socket lock */
		sofree(so);
		unp_free(unp);
		return true;
	}
	return false;
}

#ifdef notdef
unp_drain(void)
{

}
#endif

int
unp_externalize(struct mbuf *rights, struct lwp *l, int flags)
{
	struct cmsghdr * const cm = mtod(rights, struct cmsghdr *);
	struct proc * const p = l->l_proc;
	file_t **rp;
	int error = 0;

	const size_t nfds = (cm->cmsg_len - CMSG_ALIGN(sizeof(*cm))) /
	    sizeof(file_t *);
	if (nfds == 0)
		goto noop;

	int * const fdp = kmem_alloc(nfds * sizeof(int), KM_SLEEP);
	rw_enter(&p->p_cwdi->cwdi_lock, RW_READER);

	/* Make sure the recipient should be able to see the files.. */
	rp = (file_t **)CMSG_DATA(cm);
	for (size_t i = 0; i < nfds; i++) {
		file_t * const fp = *rp++;
		if (fp == NULL) {
			error = EINVAL;
			goto out;
		}
		/*
		 * If we are in a chroot'ed directory, and
		 * someone wants to pass us a directory, make
		 * sure it's inside the subtree we're allowed
		 * to access.
		 */
		if (p->p_cwdi->cwdi_rdir != NULL && fp->f_type == DTYPE_VNODE) {
			vnode_t *vp = fp->f_vnode;
			if ((vp->v_type == VDIR) &&
			    !vn_isunder(vp, p->p_cwdi->cwdi_rdir, l)) {
				error = EPERM;
				goto out;
			}
		}
	}

 restart:
	/*
	 * First loop -- allocate file descriptor table slots for the
	 * new files.
	 */
	for (size_t i = 0; i < nfds; i++) {
		if ((error = fd_alloc(p, 0, &fdp[i])) != 0) {
			/*
			 * Back out what we've done so far.
			 */
			while (i-- > 0) {
				fd_abort(p, NULL, fdp[i]);
			}
			if (error == ENOSPC) {
				fd_tryexpand(p);
				error = 0;
				goto restart;
			}
			/*
			 * This is the error that has historically
			 * been returned, and some callers may
			 * expect it.
			 */
			error = EMSGSIZE;
			goto out;
		}
	}

	/*
	 * Now that adding them has succeeded, update all of the
	 * file passing state and affix the descriptors.
	 */
	rp = (file_t **)CMSG_DATA(cm);
	int *ofdp = (int *)CMSG_DATA(cm);
	for (size_t i = 0; i < nfds; i++) {
		file_t * const fp = *rp++;
		const int fd = fdp[i];
		atomic_dec_uint(&unp_rights);
		fd_set_exclose(l, fd, (flags & O_CLOEXEC) != 0);
		fd_affix(p, fp, fd);
		/*
		 * Done with this file pointer, replace it with a fd;
		 */
		*ofdp++ = fd;
		mutex_enter(&fp->f_lock);
		fp->f_msgcount--;
		mutex_exit(&fp->f_lock);
		/*
		 * Note that fd_affix() adds a reference to the file.
		 * The file may already have been closed by another
		 * LWP in the process, so we must drop the reference
		 * added by unp_internalize() with closef().
		 */
		closef(fp);
	}

	/*
	 * Adjust length, in case of transition from large file_t
	 * pointers to ints.
	 */
	if (sizeof(file_t *) != sizeof(int)) {
		cm->cmsg_len = CMSG_LEN(nfds * sizeof(int));
		rights->m_len = CMSG_SPACE(nfds * sizeof(int));
	}
 out:
	if (__predict_false(error != 0)) {
		file_t **const fpp = (file_t **)CMSG_DATA(cm);
		for (size_t i = 0; i < nfds; i++)
			unp_discard_now(fpp[i]);
		/*
		 * Truncate the array so that nobody will try to interpret
		 * what is now garbage in it.
		 */
		cm->cmsg_len = CMSG_LEN(0);
		rights->m_len = CMSG_SPACE(0);
	}
	rw_exit(&p->p_cwdi->cwdi_lock);
	kmem_free(fdp, nfds * sizeof(int));

 noop:
	/*
	 * Don't disclose kernel memory in the alignment space.
	 */
	KASSERT(cm->cmsg_len <= rights->m_len);
	memset(&mtod(rights, char *)[cm->cmsg_len], 0, rights->m_len -
	    cm->cmsg_len);
	return error;
}

static int
unp_internalize(struct mbuf **controlp)
{
	filedesc_t *fdescp = curlwp->l_fd;
	struct mbuf *control = *controlp;
	struct cmsghdr *newcm, *cm = mtod(control, struct cmsghdr *);
	file_t **rp, **files;
	file_t *fp;
	int i, fd, *fdp;
	int nfds, error;
	u_int maxmsg;

	error = 0;
	newcm = NULL;

	/* Sanity check the control message header. */
	if (cm->cmsg_type != SCM_RIGHTS || cm->cmsg_level != SOL_SOCKET ||
	    cm->cmsg_len > control->m_len ||
	    cm->cmsg_len < CMSG_ALIGN(sizeof(*cm)))
		return (EINVAL);

	/*
	 * Verify that the file descriptors are valid, and acquire
	 * a reference to each.
	 */
	nfds = (cm->cmsg_len - CMSG_ALIGN(sizeof(*cm))) / sizeof(int);
	fdp = (int *)CMSG_DATA(cm);
	maxmsg = maxfiles / unp_rights_ratio;
	for (i = 0; i < nfds; i++) {
		fd = *fdp++;
		if (atomic_inc_uint_nv(&unp_rights) > maxmsg) {
			atomic_dec_uint(&unp_rights);
			nfds = i;
			error = EAGAIN;
			goto out;
		}
		if ((fp = fd_getfile(fd)) == NULL
		    || fp->f_type == DTYPE_KQUEUE) {
		    	if (fp)
		    		fd_putfile(fd);
			atomic_dec_uint(&unp_rights);
			nfds = i;
			error = EBADF;
			goto out;
		}
	}

	/* Allocate new space and copy header into it. */
	newcm = malloc(CMSG_SPACE(nfds * sizeof(file_t *)), M_MBUF, M_WAITOK);
	if (newcm == NULL) {
		error = E2BIG;
		goto out;
	}
	memcpy(newcm, cm, sizeof(struct cmsghdr));
	files = (file_t **)CMSG_DATA(newcm);

	/*
	 * Transform the file descriptors into file_t pointers, in
	 * reverse order so that if pointers are bigger than ints, the
	 * int won't get until we're done.  No need to lock, as we have
	 * already validated the descriptors with fd_getfile().
	 */
	fdp = (int *)CMSG_DATA(cm) + nfds;
	rp = files + nfds;
	for (i = 0; i < nfds; i++) {
		fp = fdescp->fd_dt->dt_ff[*--fdp]->ff_file;
		KASSERT(fp != NULL);
		mutex_enter(&fp->f_lock);
		*--rp = fp;
		fp->f_count++;
		fp->f_msgcount++;
		mutex_exit(&fp->f_lock);
	}

 out:
 	/* Release descriptor references. */
	fdp = (int *)CMSG_DATA(cm);
	for (i = 0; i < nfds; i++) {
		fd_putfile(*fdp++);
		if (error != 0) {
			atomic_dec_uint(&unp_rights);
		}
	}

	if (error == 0) {
		if (control->m_flags & M_EXT) {
			m_freem(control);
			*controlp = control = m_get(M_WAIT, MT_CONTROL);
		}
		MEXTADD(control, newcm, CMSG_SPACE(nfds * sizeof(file_t *)),
		    M_MBUF, NULL, NULL);
		cm = newcm;
		/*
		 * Adjust message & mbuf to note amount of space
		 * actually used.
		 */
		cm->cmsg_len = CMSG_LEN(nfds * sizeof(file_t *));
		control->m_len = CMSG_SPACE(nfds * sizeof(file_t *));
	}

	return error;
}

struct mbuf *
unp_addsockcred(struct lwp *l, struct mbuf *control)
{
	struct sockcred *sc;
	struct mbuf *m;
	void *p;

	m = sbcreatecontrol1(&p, SOCKCREDSIZE(kauth_cred_ngroups(l->l_cred)),
		SCM_CREDS, SOL_SOCKET, M_WAITOK);
	if (m == NULL)
		return control;
		
	sc = p;
	sc->sc_uid = kauth_cred_getuid(l->l_cred);
	sc->sc_euid = kauth_cred_geteuid(l->l_cred);
	sc->sc_gid = kauth_cred_getgid(l->l_cred);
	sc->sc_egid = kauth_cred_getegid(l->l_cred);
	sc->sc_ngroups = kauth_cred_ngroups(l->l_cred);

	for (int i = 0; i < sc->sc_ngroups; i++)
		sc->sc_groups[i] = kauth_cred_group(l->l_cred, i);

	return m_add(control, m);
}

/*
 * Do a mark-sweep GC of files in the system, to free up any which are
 * caught in flight to an about-to-be-closed socket.  Additionally,
 * process deferred file closures.
 */
static void
unp_gc(file_t *dp)
{
	extern	struct domain unixdomain;
	file_t *fp, *np;
	struct socket *so, *so1;
	u_int i, oflags, rflags;
	bool didwork;

	KASSERT(curlwp == unp_thread_lwp);
	KASSERT(mutex_owned(&filelist_lock));

	/*
	 * First, process deferred file closures.
	 */
	while (!SLIST_EMPTY(&unp_thread_discard)) {
		fp = SLIST_FIRST(&unp_thread_discard);
		KASSERT(fp->f_unpcount > 0);
		KASSERT(fp->f_count > 0);
		KASSERT(fp->f_msgcount > 0);
		KASSERT(fp->f_count >= fp->f_unpcount);
		KASSERT(fp->f_count >= fp->f_msgcount);
		KASSERT(fp->f_msgcount >= fp->f_unpcount);
		SLIST_REMOVE_HEAD(&unp_thread_discard, f_unplist);
		i = fp->f_unpcount;
		fp->f_unpcount = 0;
		mutex_exit(&filelist_lock);
		for (; i != 0; i--) {
			unp_discard_now(fp);
		}
		mutex_enter(&filelist_lock);
	}

	/*
	 * Clear mark bits.  Ensure that we don't consider new files
	 * entering the file table during this loop (they will not have
	 * FSCAN set).
	 */
	unp_defer = 0;
	LIST_FOREACH(fp, &filehead, f_list) {
		for (oflags = fp->f_flag;; oflags = rflags) {
			rflags = atomic_cas_uint(&fp->f_flag, oflags,
			    (oflags | FSCAN) & ~(FMARK|FDEFER));
			if (__predict_true(oflags == rflags)) {
				break;
			}
		}
	}

	/*
	 * Iterate over the set of sockets, marking ones believed (based on
	 * refcount) to be referenced from a process, and marking for rescan
	 * sockets which are queued on a socket.  Recan continues descending
	 * and searching for sockets referenced by sockets (FDEFER), until
	 * there are no more socket->socket references to be discovered.
	 */
	do {
		didwork = false;
		for (fp = LIST_FIRST(&filehead); fp != NULL; fp = np) {
			KASSERT(mutex_owned(&filelist_lock));
			np = LIST_NEXT(fp, f_list);
			mutex_enter(&fp->f_lock);
			if ((fp->f_flag & FDEFER) != 0) {
				atomic_and_uint(&fp->f_flag, ~FDEFER);
				unp_defer--;
				if (fp->f_count == 0) {
					/*
					 * XXX: closef() doesn't pay attention
					 * to FDEFER
					 */
					mutex_exit(&fp->f_lock);
					continue;
				}
			} else {
				if (fp->f_count == 0 ||
				    (fp->f_flag & FMARK) != 0 ||
				    fp->f_count == fp->f_msgcount ||
				    fp->f_unpcount != 0) {
					mutex_exit(&fp->f_lock);
					continue;
				}
			}
			atomic_or_uint(&fp->f_flag, FMARK);

			if (fp->f_type != DTYPE_SOCKET ||
			    (so = fp->f_socket) == NULL ||
			    so->so_proto->pr_domain != &unixdomain ||
			    (so->so_proto->pr_flags & PR_RIGHTS) == 0) {
				mutex_exit(&fp->f_lock);
				continue;
			}

			/* Gain file ref, mark our position, and unlock. */
			didwork = true;
			LIST_INSERT_AFTER(fp, dp, f_list);
			fp->f_count++;
			mutex_exit(&fp->f_lock);
			mutex_exit(&filelist_lock);

			/*
			 * Mark files referenced from sockets queued on the
			 * accept queue as well.
			 */
			solock(so);
			unp_scan(so->so_rcv.sb_mb, unp_mark, 0);
			if ((so->so_options & SO_ACCEPTCONN) != 0) {
				TAILQ_FOREACH(so1, &so->so_q0, so_qe) {
					unp_scan(so1->so_rcv.sb_mb, unp_mark, 0);
				}
				TAILQ_FOREACH(so1, &so->so_q, so_qe) {
					unp_scan(so1->so_rcv.sb_mb, unp_mark, 0);
				}
			}
			sounlock(so);

			/* Re-lock and restart from where we left off. */
			closef(fp);
			mutex_enter(&filelist_lock);
			np = LIST_NEXT(dp, f_list);
			LIST_REMOVE(dp, f_list);
		}
		/*
		 * Bail early if we did nothing in the loop above.  Could
		 * happen because of concurrent activity causing unp_defer
		 * to get out of sync.
		 */
	} while (unp_defer != 0 && didwork);

	/*
	 * Sweep pass.
	 *
	 * We grab an extra reference to each of the files that are
	 * not otherwise accessible and then free the rights that are
	 * stored in messages on them.
	 */
	for (fp = LIST_FIRST(&filehead); fp != NULL; fp = np) {
		KASSERT(mutex_owned(&filelist_lock));
		np = LIST_NEXT(fp, f_list);
		mutex_enter(&fp->f_lock);

		/*
		 * Ignore non-sockets.
		 * Ignore dead sockets, or sockets with pending close.
		 * Ignore sockets obviously referenced elsewhere. 
		 * Ignore sockets marked as referenced by our scan.
		 * Ignore new sockets that did not exist during the scan.
		 */
		if (fp->f_type != DTYPE_SOCKET ||
		    fp->f_count == 0 || fp->f_unpcount != 0 ||
		    fp->f_count != fp->f_msgcount ||
		    (fp->f_flag & (FMARK | FSCAN)) != FSCAN) {
			mutex_exit(&fp->f_lock);
			continue;
		}

		/* Gain file ref, mark our position, and unlock. */
		LIST_INSERT_AFTER(fp, dp, f_list);
		fp->f_count++;
		mutex_exit(&fp->f_lock);
		mutex_exit(&filelist_lock);

		/*
		 * Flush all data from the socket's receive buffer.
		 * This will cause files referenced only by the
		 * socket to be queued for close.
		 */
		so = fp->f_socket;
		solock(so);
		sorflush(so);
		sounlock(so);

		/* Re-lock and restart from where we left off. */
		closef(fp);
		mutex_enter(&filelist_lock);
		np = LIST_NEXT(dp, f_list);
		LIST_REMOVE(dp, f_list);
	}
}

/*
 * Garbage collector thread.  While SCM_RIGHTS messages are in transit,
 * wake once per second to garbage collect.  Run continually while we
 * have deferred closes to process.
 */
static void
unp_thread(void *cookie)
{
	file_t *dp;

	/* Allocate a dummy file for our scans. */
	if ((dp = fgetdummy()) == NULL) {
		panic("unp_thread");
	}

	mutex_enter(&filelist_lock);
	for (;;) {
		KASSERT(mutex_owned(&filelist_lock));
		if (SLIST_EMPTY(&unp_thread_discard)) {
			if (unp_rights != 0) {
				(void)cv_timedwait(&unp_thread_cv,
				    &filelist_lock, hz);
			} else {
				cv_wait(&unp_thread_cv, &filelist_lock);
			}
		}
		unp_gc(dp);
	}
	/* NOTREACHED */
}

/*
 * Kick the garbage collector into action if there is something for
 * it to process.
 */
static void
unp_thread_kick(void)
{

	if (!SLIST_EMPTY(&unp_thread_discard) || unp_rights != 0) {
		mutex_enter(&filelist_lock);
		cv_signal(&unp_thread_cv);
		mutex_exit(&filelist_lock);
	}
}

void
unp_dispose(struct mbuf *m)
{

	if (m)
		unp_scan(m, unp_discard_later, 1);
}

void
unp_scan(struct mbuf *m0, void (*op)(file_t *), int discard)
{
	struct mbuf *m;
	file_t **rp, *fp;
	struct cmsghdr *cm;
	int i, qfds;

	while (m0) {
		for (m = m0; m; m = m->m_next) {
			if (m->m_type != MT_CONTROL ||
			    m->m_len < sizeof(*cm)) {
			    	continue;
			}
			cm = mtod(m, struct cmsghdr *);
			if (cm->cmsg_level != SOL_SOCKET ||
			    cm->cmsg_type != SCM_RIGHTS)
				continue;
			qfds = (cm->cmsg_len - CMSG_ALIGN(sizeof(*cm)))
			    / sizeof(file_t *);
			rp = (file_t **)CMSG_DATA(cm);
			for (i = 0; i < qfds; i++) {
				fp = *rp;
				if (discard) {
					*rp = 0;
				}
				(*op)(fp);
				rp++;
			}
		}
		m0 = m0->m_nextpkt;
	}
}

void
unp_mark(file_t *fp)
{

	if (fp == NULL)
		return;

	/* If we're already deferred, don't screw up the defer count */
	mutex_enter(&fp->f_lock);
	if (fp->f_flag & (FMARK | FDEFER)) {
		mutex_exit(&fp->f_lock);
		return;
	}

	/*
	 * Minimize the number of deferrals...  Sockets are the only type of
	 * file which can hold references to another file, so just mark
	 * other files, and defer unmarked sockets for the next pass.
	 */
	if (fp->f_type == DTYPE_SOCKET) {
		unp_defer++;
		KASSERT(fp->f_count != 0);
		atomic_or_uint(&fp->f_flag, FDEFER);
	} else {
		atomic_or_uint(&fp->f_flag, FMARK);
	}
	mutex_exit(&fp->f_lock);
}

static void
unp_discard_now(file_t *fp)
{

	if (fp == NULL)
		return;

	KASSERT(fp->f_count > 0);
	KASSERT(fp->f_msgcount > 0);

	mutex_enter(&fp->f_lock);
	fp->f_msgcount--;
	mutex_exit(&fp->f_lock);
	atomic_dec_uint(&unp_rights);
	(void)closef(fp);
}

static void
unp_discard_later(file_t *fp)
{

	if (fp == NULL)
		return;

	KASSERT(fp->f_count > 0);
	KASSERT(fp->f_msgcount > 0);

	mutex_enter(&filelist_lock);
	if (fp->f_unpcount++ == 0) {
		SLIST_INSERT_HEAD(&unp_thread_discard, fp, f_unplist);
	}
	mutex_exit(&filelist_lock);
}

const struct pr_usrreqs unp_usrreqs = {
	.pr_attach	= unp_attach,
	.pr_detach	= unp_detach,
	.pr_accept	= unp_accept,
	.pr_bind	= unp_bind,
	.pr_listen	= unp_listen,
	.pr_connect	= unp_connect,
	.pr_connect2	= unp_connect2,
	.pr_disconnect	= unp_disconnect,
	.pr_shutdown	= unp_shutdown,
	.pr_abort	= unp_abort,
	.pr_ioctl	= unp_ioctl,
	.pr_stat	= unp_stat,
	.pr_peeraddr	= unp_peeraddr,
	.pr_sockaddr	= unp_sockaddr,
	.pr_rcvd	= unp_rcvd,
	.pr_recvoob	= unp_recvoob,
	.pr_send	= unp_send,
	.pr_sendoob	= unp_sendoob,
};
