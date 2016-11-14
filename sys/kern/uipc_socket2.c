/*	$NetBSD: uipc_socket2.c,v 1.122 2015/08/24 22:21:26 pooka Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)uipc_socket2.c	8.2 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_socket2.c,v 1.122 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_mbuftrace.h"
#include "opt_sb_max.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>
#include <sys/pool.h>
#include <sys/uidinfo.h>

/*
 * Primitive routines for operating on sockets and socket buffers.
 *
 * Connection life-cycle:
 *
 *	Normal sequence from the active (originating) side:
 *
 *	- soisconnecting() is called during processing of connect() call,
 *	- resulting in an eventual call to soisconnected() if/when the
 *	  connection is established.
 *
 *	When the connection is torn down during processing of disconnect():
 *
 *	- soisdisconnecting() is called and,
 *	- soisdisconnected() is called when the connection to the peer
 *	  is totally severed.
 *
 *	The semantics of these routines are such that connectionless protocols
 *	can call soisconnected() and soisdisconnected() only, bypassing the
 *	in-progress calls when setting up a ``connection'' takes no time.
 *
 *	From the passive side, a socket is created with two queues of sockets:
 *
 *	- so_q0 (0) for partial connections (i.e. connections in progress)
 *	- so_q (1) for connections already made and awaiting user acceptance.
 *
 *	As a protocol is preparing incoming connections, it creates a socket
 *	structure queued on so_q0 by calling sonewconn().  When the connection
 *	is established, soisconnected() is called, and transfers the
 *	socket structure to so_q, making it available to accept().
 *
 *	If a socket is closed with sockets on either so_q0 or so_q, these
 *	sockets are dropped.
 *
 * Locking rules and assumptions:
 *
 * o socket::so_lock can change on the fly.  The low level routines used
 *   to lock sockets are aware of this.  When so_lock is acquired, the
 *   routine locking must check to see if so_lock still points to the
 *   lock that was acquired.  If so_lock has changed in the meantime, the
 *   now irrelevant lock that was acquired must be dropped and the lock
 *   operation retried.  Although not proven here, this is completely safe
 *   on a multiprocessor system, even with relaxed memory ordering, given
 *   the next two rules:
 *
 * o In order to mutate so_lock, the lock pointed to by the current value
 *   of so_lock must be held: i.e., the socket must be held locked by the
 *   changing thread.  The thread must issue membar_exit() to prevent
 *   memory accesses being reordered, and can set so_lock to the desired
 *   value.  If the lock pointed to by the new value of so_lock is not
 *   held by the changing thread, the socket must then be considered
 *   unlocked.
 *
 * o If so_lock is mutated, and the previous lock referred to by so_lock
 *   could still be visible to other threads in the system (e.g. via file
 *   descriptor or protocol-internal reference), then the old lock must
 *   remain valid until the socket and/or protocol control block has been
 *   torn down.
 *
 * o If a socket has a non-NULL so_head value (i.e. is in the process of
 *   connecting), then locking the socket must also lock the socket pointed
 *   to by so_head: their lock pointers must match.
 *
 * o If a socket has connections in progress (so_q, so_q0 not empty) then
 *   locking the socket must also lock the sockets attached to both queues.
 *   Again, their lock pointers must match.
 *
 * o Beyond the initial lock assignment in socreate(), assigning locks to
 *   sockets is the responsibility of the individual protocols / protocol
 *   domains.
 */

static pool_cache_t	socket_cache;
u_long			sb_max = SB_MAX;/* maximum socket buffer size */
static u_long		sb_max_adj;	/* adjusted sb_max */

void
soisconnecting(struct socket *so)
{

	KASSERT(solocked(so));

	so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTING;
}

void
soisconnected(struct socket *so)
{
	struct socket	*head;

	head = so->so_head;

	KASSERT(solocked(so));
	KASSERT(head == NULL || solocked2(so, head));

	so->so_state &= ~(SS_ISCONNECTING | SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTED;
	if (head && so->so_onq == &head->so_q0) {
		if ((so->so_options & SO_ACCEPTFILTER) == 0) {
			/*
			 * Re-enqueue and wake up any waiters, e.g.
			 * processes blocking on accept().
			 */
			soqremque(so, 0);
			soqinsque(head, so, 1);
			sorwakeup(head);
			cv_broadcast(&head->so_cv);
		} else {
			so->so_upcall =
			    head->so_accf->so_accept_filter->accf_callback;
			so->so_upcallarg = head->so_accf->so_accept_filter_arg;
			so->so_rcv.sb_flags |= SB_UPCALL;
			so->so_options &= ~SO_ACCEPTFILTER;
			(*so->so_upcall)(so, so->so_upcallarg,
					 POLLIN|POLLRDNORM, M_DONTWAIT);
		}
	} else {
		cv_broadcast(&so->so_cv);
		sorwakeup(so);
		sowwakeup(so);
	}
}

void
soisdisconnecting(struct socket *so)
{

	KASSERT(solocked(so));

	so->so_state &= ~SS_ISCONNECTING;
	so->so_state |= (SS_ISDISCONNECTING|SS_CANTRCVMORE|SS_CANTSENDMORE);
	cv_broadcast(&so->so_cv);
	sowwakeup(so);
	sorwakeup(so);
}

void
soisdisconnected(struct socket *so)
{

	KASSERT(solocked(so));

	so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE|SS_ISDISCONNECTED);
	cv_broadcast(&so->so_cv);
	sowwakeup(so);
	sorwakeup(so);
}

void
soinit2(void)
{

	socket_cache = pool_cache_init(sizeof(struct socket), 0, 0, 0,
	    "socket", NULL, IPL_SOFTNET, NULL, NULL, NULL);
}

/*
 * sonewconn: accept a new connection.
 *
 * When an attempt at a new connection is noted on a socket which accepts
 * connections, sonewconn(9) is called.  If the connection is possible
 * (subject to space constraints, etc) then we allocate a new structure,
 * properly linked into the data structure of the original socket.
 *
 * => If 'soready' is true, then socket will become ready for accept() i.e.
 *    inserted into the so_q queue, SS_ISCONNECTED set and waiters awoken.
 * => May be called from soft-interrupt context.
 * => Listening socket should be locked.
 * => Returns the new socket locked.
 */
struct socket *
sonewconn(struct socket *head, bool soready)
{
	struct socket *so;
	int soqueue, error;

	KASSERT(solocked(head));

	if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2) {
		/* Listen queue overflow. */
		return NULL;
	}
	if ((head->so_options & SO_ACCEPTFILTER) != 0) {
		soready = false;
	}
	soqueue = soready ? 1 : 0;

	if ((so = soget(false)) == NULL) {
		return NULL;
	}
	so->so_type = head->so_type;
	so->so_options = head->so_options & ~SO_ACCEPTCONN;
	so->so_linger = head->so_linger;
	so->so_state = head->so_state | SS_NOFDREF;
	so->so_proto = head->so_proto;
	so->so_timeo = head->so_timeo;
	so->so_pgid = head->so_pgid;
	so->so_send = head->so_send;
	so->so_receive = head->so_receive;
	so->so_uidinfo = head->so_uidinfo;
	so->so_cpid = head->so_cpid;

	/*
	 * Share the lock with the listening-socket, it may get unshared
	 * once the connection is complete.
	 */
	mutex_obj_hold(head->so_lock);
	so->so_lock = head->so_lock;

	/*
	 * Reserve the space for socket buffers.
	 */
#ifdef MBUFTRACE
	so->so_mowner = head->so_mowner;
	so->so_rcv.sb_mowner = head->so_rcv.sb_mowner;
	so->so_snd.sb_mowner = head->so_snd.sb_mowner;
#endif
	if (soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat)) {
		goto out;
	}
	so->so_snd.sb_lowat = head->so_snd.sb_lowat;
	so->so_rcv.sb_lowat = head->so_rcv.sb_lowat;
	so->so_rcv.sb_timeo = head->so_rcv.sb_timeo;
	so->so_snd.sb_timeo = head->so_snd.sb_timeo;
	so->so_rcv.sb_flags |= head->so_rcv.sb_flags & (SB_AUTOSIZE | SB_ASYNC);
	so->so_snd.sb_flags |= head->so_snd.sb_flags & (SB_AUTOSIZE | SB_ASYNC);

	/*
	 * Finally, perform the protocol attach.  Note: a new socket
	 * lock may be assigned at this point (if so, it will be held).
	 */
	error = (*so->so_proto->pr_usrreqs->pr_attach)(so, 0);
	if (error) {
out:
		KASSERT(solocked(so));
		KASSERT(so->so_accf == NULL);
		soput(so);

		/* Note: the listening socket shall stay locked. */
		KASSERT(solocked(head));
		return NULL;
	}
	KASSERT(solocked2(head, so));

	/*
	 * Insert into the queue.  If ready, update the connection status
	 * and wake up any waiters, e.g. processes blocking on accept().
	 */
	soqinsque(head, so, soqueue);
	if (soready) {
		so->so_state |= SS_ISCONNECTED;
		sorwakeup(head);
		cv_broadcast(&head->so_cv);
	}
	return so;
}

struct socket *
soget(bool waitok)
{
	struct socket *so;

	so = pool_cache_get(socket_cache, (waitok ? PR_WAITOK : PR_NOWAIT));
	if (__predict_false(so == NULL))
		return (NULL);
	memset(so, 0, sizeof(*so));
	TAILQ_INIT(&so->so_q0);
	TAILQ_INIT(&so->so_q);
	cv_init(&so->so_cv, "socket");
	cv_init(&so->so_rcv.sb_cv, "netio");
	cv_init(&so->so_snd.sb_cv, "netio");
	selinit(&so->so_rcv.sb_sel);
	selinit(&so->so_snd.sb_sel);
	so->so_rcv.sb_so = so;
	so->so_snd.sb_so = so;
	return so;
}

void
soput(struct socket *so)
{

	KASSERT(!cv_has_waiters(&so->so_cv));
	KASSERT(!cv_has_waiters(&so->so_rcv.sb_cv));
	KASSERT(!cv_has_waiters(&so->so_snd.sb_cv));
	seldestroy(&so->so_rcv.sb_sel);
	seldestroy(&so->so_snd.sb_sel);
	mutex_obj_free(so->so_lock);
	cv_destroy(&so->so_cv);
	cv_destroy(&so->so_rcv.sb_cv);
	cv_destroy(&so->so_snd.sb_cv);
	pool_cache_put(socket_cache, so);
}

/*
 * soqinsque: insert socket of a new connection into the specified
 * accept queue of the listening socket (head).
 *
 *	q = 0: queue of partial connections
 *	q = 1: queue of incoming connections
 */
void
soqinsque(struct socket *head, struct socket *so, int q)
{
	KASSERT(q == 0 || q == 1);
	KASSERT(solocked2(head, so));
	KASSERT(so->so_onq == NULL);
	KASSERT(so->so_head == NULL);

	so->so_head = head;
	if (q == 0) {
		head->so_q0len++;
		so->so_onq = &head->so_q0;
	} else {
		head->so_qlen++;
		so->so_onq = &head->so_q;
	}
	TAILQ_INSERT_TAIL(so->so_onq, so, so_qe);
}

/*
 * soqremque: remove socket from the specified queue.
 *
 * => Returns true if socket was removed from the specified queue.
 * => False if socket was not removed (because it was in other queue).
 */
bool
soqremque(struct socket *so, int q)
{
	struct socket *head = so->so_head;

	KASSERT(q == 0 || q == 1);
	KASSERT(solocked(so));
	KASSERT(so->so_onq != NULL);
	KASSERT(head != NULL);

	if (q == 0) {
		if (so->so_onq != &head->so_q0)
			return false;
		head->so_q0len--;
	} else {
		if (so->so_onq != &head->so_q)
			return false;
		head->so_qlen--;
	}
	KASSERT(solocked2(so, head));
	TAILQ_REMOVE(so->so_onq, so, so_qe);
	so->so_onq = NULL;
	so->so_head = NULL;
	return true;
}

/*
 * socantsendmore: indicates that no more data will be sent on the
 * socket; it would normally be applied to a socket when the user
 * informs the system that no more data is to be sent, by the protocol
 * code (in case pr_shutdown()).
 */
void
socantsendmore(struct socket *so)
{
	KASSERT(solocked(so));

	so->so_state |= SS_CANTSENDMORE;
	sowwakeup(so);
}

/*
 * socantrcvmore(): indicates that no more data will be received and
 * will normally be applied to the socket by a protocol when it detects
 * that the peer will send no more data.  Data queued for reading in
 * the socket may yet be read.
 */
void
socantrcvmore(struct socket *so)
{
	KASSERT(solocked(so));

	so->so_state |= SS_CANTRCVMORE;
	sorwakeup(so);
}

/*
 * Wait for data to arrive at/drain from a socket buffer.
 */
int
sbwait(struct sockbuf *sb)
{
	struct socket *so;
	kmutex_t *lock;
	int error;

	so = sb->sb_so;

	KASSERT(solocked(so));

	sb->sb_flags |= SB_NOTIFY;
	lock = so->so_lock;
	if ((sb->sb_flags & SB_NOINTR) != 0)
		error = cv_timedwait(&sb->sb_cv, lock, sb->sb_timeo);
	else
		error = cv_timedwait_sig(&sb->sb_cv, lock, sb->sb_timeo);
	if (__predict_false(lock != so->so_lock))
		solockretry(so, lock);
	return error;
}

/*
 * Wakeup processes waiting on a socket buffer.
 * Do asynchronous notification via SIGIO
 * if the socket buffer has the SB_ASYNC flag set.
 */
void
sowakeup(struct socket *so, struct sockbuf *sb, int code)
{
	int band;

	KASSERT(solocked(so));
	KASSERT(sb->sb_so == so);

	if (code == POLL_IN)
		band = POLLIN|POLLRDNORM;
	else
		band = POLLOUT|POLLWRNORM;
	sb->sb_flags &= ~SB_NOTIFY;
	selnotify(&sb->sb_sel, band, NOTE_SUBMIT);
	cv_broadcast(&sb->sb_cv);
	if (sb->sb_flags & SB_ASYNC)
		fownsignal(so->so_pgid, SIGIO, code, band, so);
	if (sb->sb_flags & SB_UPCALL)
		(*so->so_upcall)(so, so->so_upcallarg, band, M_DONTWAIT);
}

/*
 * Reset a socket's lock pointer.  Wake all threads waiting on the
 * socket's condition variables so that they can restart their waits
 * using the new lock.  The existing lock must be held.
 */
void
solockreset(struct socket *so, kmutex_t *lock)
{

	KASSERT(solocked(so));

	so->so_lock = lock;
	cv_broadcast(&so->so_snd.sb_cv);
	cv_broadcast(&so->so_rcv.sb_cv);
	cv_broadcast(&so->so_cv);
}

/*
 * Socket buffer (struct sockbuf) utility routines.
 *
 * Each socket contains two socket buffers: one for sending data and
 * one for receiving data.  Each buffer contains a queue of mbufs,
 * information about the number of mbufs and amount of data in the
 * queue, and other fields allowing poll() statements and notification
 * on data availability to be implemented.
 *
 * Data stored in a socket buffer is maintained as a list of records.
 * Each record is a list of mbufs chained together with the m_next
 * field.  Records are chained together with the m_nextpkt field. The upper
 * level routine soreceive() expects the following conventions to be
 * observed when placing information in the receive buffer:
 *
 * 1. If the protocol requires each message be preceded by the sender's
 *    name, then a record containing that name must be present before
 *    any associated data (mbuf's must be of type MT_SONAME).
 * 2. If the protocol supports the exchange of ``access rights'' (really
 *    just additional data associated with the message), and there are
 *    ``rights'' to be received, then a record containing this data
 *    should be present (mbuf's must be of type MT_CONTROL).
 * 3. If a name or rights record exists, then it must be followed by
 *    a data record, perhaps of zero length.
 *
 * Before using a new socket structure it is first necessary to reserve
 * buffer space to the socket, by calling sbreserve().  This should commit
 * some of the available buffer space in the system buffer pool for the
 * socket (currently, it does nothing but enforce limits).  The space
 * should be released by calling sbrelease() when the socket is destroyed.
 */

int
sb_max_set(u_long new_sbmax)
{
	int s;

	if (new_sbmax < (16 * 1024))
		return (EINVAL);

	s = splsoftnet();
	sb_max = new_sbmax;
	sb_max_adj = (u_quad_t)new_sbmax * MCLBYTES / (MSIZE + MCLBYTES);
	splx(s);

	return (0);
}

int
soreserve(struct socket *so, u_long sndcc, u_long rcvcc)
{
	KASSERT(so->so_pcb == NULL || solocked(so));

	/*
	 * there's at least one application (a configure script of screen)
	 * which expects a fifo is writable even if it has "some" bytes
	 * in its buffer.
	 * so we want to make sure (hiwat - lowat) >= (some bytes).
	 *
	 * PIPE_BUF here is an arbitrary value chosen as (some bytes) above.
	 * we expect it's large enough for such applications.
	 */
	u_long  lowat = MAX(sock_loan_thresh, MCLBYTES);
	u_long  hiwat = lowat + PIPE_BUF;

	if (sndcc < hiwat)
		sndcc = hiwat;
	if (sbreserve(&so->so_snd, sndcc, so) == 0)
		goto bad;
	if (sbreserve(&so->so_rcv, rcvcc, so) == 0)
		goto bad2;
	if (so->so_rcv.sb_lowat == 0)
		so->so_rcv.sb_lowat = 1;
	if (so->so_snd.sb_lowat == 0)
		so->so_snd.sb_lowat = lowat;
	if (so->so_snd.sb_lowat > so->so_snd.sb_hiwat)
		so->so_snd.sb_lowat = so->so_snd.sb_hiwat;
	return (0);
 bad2:
	sbrelease(&so->so_snd, so);
 bad:
	return (ENOBUFS);
}

/*
 * Allot mbufs to a sockbuf.
 * Attempt to scale mbmax so that mbcnt doesn't become limiting
 * if buffering efficiency is near the normal case.
 */
int
sbreserve(struct sockbuf *sb, u_long cc, struct socket *so)
{
	struct lwp *l = curlwp; /* XXX */
	rlim_t maxcc;
	struct uidinfo *uidinfo;

	KASSERT(so->so_pcb == NULL || solocked(so));
	KASSERT(sb->sb_so == so);
	KASSERT(sb_max_adj != 0);

	if (cc == 0 || cc > sb_max_adj)
		return (0);

	maxcc = l->l_proc->p_rlimit[RLIMIT_SBSIZE].rlim_cur;

	uidinfo = so->so_uidinfo;
	if (!chgsbsize(uidinfo, &sb->sb_hiwat, cc, maxcc))
		return 0;
	sb->sb_mbmax = min(cc * 2, sb_max);
	if (sb->sb_lowat > sb->sb_hiwat)
		sb->sb_lowat = sb->sb_hiwat;
	return (1);
}

/*
 * Free mbufs held by a socket, and reserved mbuf space.  We do not assert
 * that the socket is held locked here: see sorflush().
 */
void
sbrelease(struct sockbuf *sb, struct socket *so)
{

	KASSERT(sb->sb_so == so);

	sbflush(sb);
	(void)chgsbsize(so->so_uidinfo, &sb->sb_hiwat, 0, RLIM_INFINITY);
	sb->sb_mbmax = 0;
}

/*
 * Routines to add and remove
 * data from an mbuf queue.
 *
 * The routines sbappend() or sbappendrecord() are normally called to
 * append new mbufs to a socket buffer, after checking that adequate
 * space is available, comparing the function sbspace() with the amount
 * of data to be added.  sbappendrecord() differs from sbappend() in
 * that data supplied is treated as the beginning of a new record.
 * To place a sender's address, optional access rights, and data in a
 * socket receive buffer, sbappendaddr() should be used.  To place
 * access rights and data in a socket receive buffer, sbappendrights()
 * should be used.  In either case, the new data begins a new record.
 * Note that unlike sbappend() and sbappendrecord(), these routines check
 * for the caller that there will be enough space to store the data.
 * Each fails if there is not enough space, or if it cannot find mbufs
 * to store additional information in.
 *
 * Reliable protocols may use the socket send buffer to hold data
 * awaiting acknowledgement.  Data is normally copied from a socket
 * send buffer in a protocol with m_copy for output to a peer,
 * and then removing the data from the socket buffer with sbdrop()
 * or sbdroprecord() when the data is acknowledged by the peer.
 */

#ifdef SOCKBUF_DEBUG
void
sblastrecordchk(struct sockbuf *sb, const char *where)
{
	struct mbuf *m = sb->sb_mb;

	KASSERT(solocked(sb->sb_so));

	while (m && m->m_nextpkt)
		m = m->m_nextpkt;

	if (m != sb->sb_lastrecord) {
		printf("sblastrecordchk: sb_mb %p sb_lastrecord %p last %p\n",
		    sb->sb_mb, sb->sb_lastrecord, m);
		printf("packet chain:\n");
		for (m = sb->sb_mb; m != NULL; m = m->m_nextpkt)
			printf("\t%p\n", m);
		panic("sblastrecordchk from %s", where);
	}
}

void
sblastmbufchk(struct sockbuf *sb, const char *where)
{
	struct mbuf *m = sb->sb_mb;
	struct mbuf *n;

	KASSERT(solocked(sb->sb_so));

	while (m && m->m_nextpkt)
		m = m->m_nextpkt;

	while (m && m->m_next)
		m = m->m_next;

	if (m != sb->sb_mbtail) {
		printf("sblastmbufchk: sb_mb %p sb_mbtail %p last %p\n",
		    sb->sb_mb, sb->sb_mbtail, m);
		printf("packet tree:\n");
		for (m = sb->sb_mb; m != NULL; m = m->m_nextpkt) {
			printf("\t");
			for (n = m; n != NULL; n = n->m_next)
				printf("%p ", n);
			printf("\n");
		}
		panic("sblastmbufchk from %s", where);
	}
}
#endif /* SOCKBUF_DEBUG */

/*
 * Link a chain of records onto a socket buffer
 */
#define	SBLINKRECORDCHAIN(sb, m0, mlast)				\
do {									\
	if ((sb)->sb_lastrecord != NULL)				\
		(sb)->sb_lastrecord->m_nextpkt = (m0);			\
	else								\
		(sb)->sb_mb = (m0);					\
	(sb)->sb_lastrecord = (mlast);					\
} while (/*CONSTCOND*/0)


#define	SBLINKRECORD(sb, m0)						\
    SBLINKRECORDCHAIN(sb, m0, m0)

/*
 * Append mbuf chain m to the last record in the
 * socket buffer sb.  The additional space associated
 * the mbuf chain is recorded in sb.  Empty mbufs are
 * discarded and mbufs are compacted where possible.
 */
void
sbappend(struct sockbuf *sb, struct mbuf *m)
{
	struct mbuf	*n;

	KASSERT(solocked(sb->sb_so));

	if (m == NULL)
		return;

#ifdef MBUFTRACE
	m_claimm(m, sb->sb_mowner);
#endif

	SBLASTRECORDCHK(sb, "sbappend 1");

	if ((n = sb->sb_lastrecord) != NULL) {
		/*
		 * XXX Would like to simply use sb_mbtail here, but
		 * XXX I need to verify that I won't miss an EOR that
		 * XXX way.
		 */
		do {
			if (n->m_flags & M_EOR) {
				sbappendrecord(sb, m); /* XXXXXX!!!! */
				return;
			}
		} while (n->m_next && (n = n->m_next));
	} else {
		/*
		 * If this is the first record in the socket buffer, it's
		 * also the last record.
		 */
		sb->sb_lastrecord = m;
	}
	sbcompress(sb, m, n);
	SBLASTRECORDCHK(sb, "sbappend 2");
}

/*
 * This version of sbappend() should only be used when the caller
 * absolutely knows that there will never be more than one record
 * in the socket buffer, that is, a stream protocol (such as TCP).
 */
void
sbappendstream(struct sockbuf *sb, struct mbuf *m)
{

	KASSERT(solocked(sb->sb_so));
	KDASSERT(m->m_nextpkt == NULL);
	KASSERT(sb->sb_mb == sb->sb_lastrecord);

	SBLASTMBUFCHK(sb, __func__);

#ifdef MBUFTRACE
	m_claimm(m, sb->sb_mowner);
#endif

	sbcompress(sb, m, sb->sb_mbtail);

	sb->sb_lastrecord = sb->sb_mb;
	SBLASTRECORDCHK(sb, __func__);
}

#ifdef SOCKBUF_DEBUG
void
sbcheck(struct sockbuf *sb)
{
	struct mbuf	*m, *m2;
	u_long		len, mbcnt;

	KASSERT(solocked(sb->sb_so));

	len = 0;
	mbcnt = 0;
	for (m = sb->sb_mb; m; m = m->m_nextpkt) {
		for (m2 = m; m2 != NULL; m2 = m2->m_next) {
			len += m2->m_len;
			mbcnt += MSIZE;
			if (m2->m_flags & M_EXT)
				mbcnt += m2->m_ext.ext_size;
			if (m2->m_nextpkt != NULL)
				panic("sbcheck nextpkt");
		}
	}
	if (len != sb->sb_cc || mbcnt != sb->sb_mbcnt) {
		printf("cc %lu != %lu || mbcnt %lu != %lu\n", len, sb->sb_cc,
		    mbcnt, sb->sb_mbcnt);
		panic("sbcheck");
	}
}
#endif

/*
 * As above, except the mbuf chain
 * begins a new record.
 */
void
sbappendrecord(struct sockbuf *sb, struct mbuf *m0)
{
	struct mbuf	*m;

	KASSERT(solocked(sb->sb_so));

	if (m0 == NULL)
		return;

#ifdef MBUFTRACE
	m_claimm(m0, sb->sb_mowner);
#endif
	/*
	 * Put the first mbuf on the queue.
	 * Note this permits zero length records.
	 */
	sballoc(sb, m0);
	SBLASTRECORDCHK(sb, "sbappendrecord 1");
	SBLINKRECORD(sb, m0);
	m = m0->m_next;
	m0->m_next = 0;
	if (m && (m0->m_flags & M_EOR)) {
		m0->m_flags &= ~M_EOR;
		m->m_flags |= M_EOR;
	}
	sbcompress(sb, m, m0);
	SBLASTRECORDCHK(sb, "sbappendrecord 2");
}

/*
 * As above except that OOB data
 * is inserted at the beginning of the sockbuf,
 * but after any other OOB data.
 */
void
sbinsertoob(struct sockbuf *sb, struct mbuf *m0)
{
	struct mbuf	*m, **mp;

	KASSERT(solocked(sb->sb_so));

	if (m0 == NULL)
		return;

	SBLASTRECORDCHK(sb, "sbinsertoob 1");

	for (mp = &sb->sb_mb; (m = *mp) != NULL; mp = &((*mp)->m_nextpkt)) {
	    again:
		switch (m->m_type) {

		case MT_OOBDATA:
			continue;		/* WANT next train */

		case MT_CONTROL:
			if ((m = m->m_next) != NULL)
				goto again;	/* inspect THIS train further */
		}
		break;
	}
	/*
	 * Put the first mbuf on the queue.
	 * Note this permits zero length records.
	 */
	sballoc(sb, m0);
	m0->m_nextpkt = *mp;
	if (*mp == NULL) {
		/* m0 is actually the new tail */
		sb->sb_lastrecord = m0;
	}
	*mp = m0;
	m = m0->m_next;
	m0->m_next = 0;
	if (m && (m0->m_flags & M_EOR)) {
		m0->m_flags &= ~M_EOR;
		m->m_flags |= M_EOR;
	}
	sbcompress(sb, m, m0);
	SBLASTRECORDCHK(sb, "sbinsertoob 2");
}

/*
 * Append address and data, and optionally, control (ancillary) data
 * to the receive queue of a socket.  If present,
 * m0 must include a packet header with total length.
 * Returns 0 if no space in sockbuf or insufficient mbufs.
 */
int
sbappendaddr(struct sockbuf *sb, const struct sockaddr *asa, struct mbuf *m0,
	struct mbuf *control)
{
	struct mbuf	*m, *n, *nlast;
	int		space, len;

	KASSERT(solocked(sb->sb_so));

	space = asa->sa_len;

	if (m0 != NULL) {
		if ((m0->m_flags & M_PKTHDR) == 0)
			panic("sbappendaddr");
		space += m0->m_pkthdr.len;
#ifdef MBUFTRACE
		m_claimm(m0, sb->sb_mowner);
#endif
	}
	for (n = control; n; n = n->m_next) {
		space += n->m_len;
		MCLAIM(n, sb->sb_mowner);
		if (n->m_next == NULL)	/* keep pointer to last control buf */
			break;
	}
	if (space > sbspace(sb))
		return (0);
	m = m_get(M_DONTWAIT, MT_SONAME);
	if (m == NULL)
		return (0);
	MCLAIM(m, sb->sb_mowner);
	/*
	 * XXX avoid 'comparison always true' warning which isn't easily
	 * avoided.
	 */
	len = asa->sa_len;
	if (len > MLEN) {
		MEXTMALLOC(m, asa->sa_len, M_NOWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return (0);
		}
	}
	m->m_len = asa->sa_len;
	memcpy(mtod(m, void *), asa, asa->sa_len);
	if (n)
		n->m_next = m0;		/* concatenate data to control */
	else
		control = m0;
	m->m_next = control;

	SBLASTRECORDCHK(sb, "sbappendaddr 1");

	for (n = m; n->m_next != NULL; n = n->m_next)
		sballoc(sb, n);
	sballoc(sb, n);
	nlast = n;
	SBLINKRECORD(sb, m);

	sb->sb_mbtail = nlast;
	SBLASTMBUFCHK(sb, "sbappendaddr");
	SBLASTRECORDCHK(sb, "sbappendaddr 2");

	return (1);
}

/*
 * Helper for sbappendchainaddr: prepend a struct sockaddr* to
 * an mbuf chain.
 */
static inline struct mbuf *
m_prepend_sockaddr(struct sockbuf *sb, struct mbuf *m0,
		   const struct sockaddr *asa)
{
	struct mbuf *m;
	const int salen = asa->sa_len;

	KASSERT(solocked(sb->sb_so));

	/* only the first in each chain need be a pkthdr */
	m = m_gethdr(M_DONTWAIT, MT_SONAME);
	if (m == NULL)
		return NULL;
	MCLAIM(m, sb->sb_mowner);
#ifdef notyet
	if (salen > MHLEN) {
		MEXTMALLOC(m, salen, M_NOWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return NULL;
		}
	}
#else
	KASSERT(salen <= MHLEN);
#endif
	m->m_len = salen;
	memcpy(mtod(m, void *), asa, salen);
	m->m_next = m0;
	m->m_pkthdr.len = salen + m0->m_pkthdr.len;

	return m;
}

int
sbappendaddrchain(struct sockbuf *sb, const struct sockaddr *asa,
		  struct mbuf *m0, int sbprio)
{
	struct mbuf *m, *n, *n0, *nlast;
	int error;

	KASSERT(solocked(sb->sb_so));

	/*
	 * XXX sbprio reserved for encoding priority of this* request:
	 *  SB_PRIO_NONE --> honour normal sb limits
	 *  SB_PRIO_ONESHOT_OVERFLOW --> if socket has any space,
	 *	take whole chain. Intended for large requests
	 *      that should be delivered atomically (all, or none).
	 * SB_PRIO_OVERDRAFT -- allow a small (2*MLEN) overflow
	 *       over normal socket limits, for messages indicating
	 *       buffer overflow in earlier normal/lower-priority messages
	 * SB_PRIO_BESTEFFORT -->  ignore limits entirely.
	 *       Intended for  kernel-generated messages only.
	 *        Up to generator to avoid total mbuf resource exhaustion.
	 */
	(void)sbprio;

	if (m0 && (m0->m_flags & M_PKTHDR) == 0)
		panic("sbappendaddrchain");

#ifdef notyet
	space = sbspace(sb);

	/*
	 * Enforce SB_PRIO_* limits as described above.
	 */
#endif

	n0 = NULL;
	nlast = NULL;
	for (m = m0; m; m = m->m_nextpkt) {
		struct mbuf *np;

#ifdef MBUFTRACE
		m_claimm(m, sb->sb_mowner);
#endif

		/* Prepend sockaddr to this record (m) of input chain m0 */
	  	n = m_prepend_sockaddr(sb, m, asa);
		if (n == NULL) {
			error = ENOBUFS;
			goto bad;
		}

		/* Append record (asa+m) to end of new chain n0 */
		if (n0 == NULL) {
			n0 = n;
		} else {
			nlast->m_nextpkt = n;
		}
		/* Keep track of last record on new chain */
		nlast = n;

		for (np = n; np; np = np->m_next)
			sballoc(sb, np);
	}

	SBLASTRECORDCHK(sb, "sbappendaddrchain 1");

	/* Drop the entire chain of (asa+m) records onto the socket */
	SBLINKRECORDCHAIN(sb, n0, nlast);

	SBLASTRECORDCHK(sb, "sbappendaddrchain 2");

	for (m = nlast; m->m_next; m = m->m_next)
		;
	sb->sb_mbtail = m;
	SBLASTMBUFCHK(sb, "sbappendaddrchain");

	return (1);

bad:
	/*
	 * On error, free the prepended addreseses. For consistency
	 * with sbappendaddr(), leave it to our caller to free
	 * the input record chain passed to us as m0.
	 */
	while ((n = n0) != NULL) {
	  	struct mbuf *np;

		/* Undo the sballoc() of this record */
		for (np = n; np; np = np->m_next)
			sbfree(sb, np);

		n0 = n->m_nextpkt;	/* iterate at next prepended address */
		MFREE(n, np);		/* free prepended address (not data) */
	}
	return error;
}


int
sbappendcontrol(struct sockbuf *sb, struct mbuf *m0, struct mbuf *control)
{
	struct mbuf	*m, *mlast, *n;
	int		space;

	KASSERT(solocked(sb->sb_so));

	space = 0;
	if (control == NULL)
		panic("sbappendcontrol");
	for (m = control; ; m = m->m_next) {
		space += m->m_len;
		MCLAIM(m, sb->sb_mowner);
		if (m->m_next == NULL)
			break;
	}
	n = m;			/* save pointer to last control buffer */
	for (m = m0; m; m = m->m_next) {
		MCLAIM(m, sb->sb_mowner);
		space += m->m_len;
	}
	if (space > sbspace(sb))
		return (0);
	n->m_next = m0;			/* concatenate data to control */

	SBLASTRECORDCHK(sb, "sbappendcontrol 1");

	for (m = control; m->m_next != NULL; m = m->m_next)
		sballoc(sb, m);
	sballoc(sb, m);
	mlast = m;
	SBLINKRECORD(sb, control);

	sb->sb_mbtail = mlast;
	SBLASTMBUFCHK(sb, "sbappendcontrol");
	SBLASTRECORDCHK(sb, "sbappendcontrol 2");

	return (1);
}

/*
 * Compress mbuf chain m into the socket
 * buffer sb following mbuf n.  If n
 * is null, the buffer is presumed empty.
 */
void
sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n)
{
	int		eor;
	struct mbuf	*o;

	KASSERT(solocked(sb->sb_so));

	eor = 0;
	while (m) {
		eor |= m->m_flags & M_EOR;
		if (m->m_len == 0 &&
		    (eor == 0 ||
		     (((o = m->m_next) || (o = n)) &&
		      o->m_type == m->m_type))) {
			if (sb->sb_lastrecord == m)
				sb->sb_lastrecord = m->m_next;
			m = m_free(m);
			continue;
		}
		if (n && (n->m_flags & M_EOR) == 0 &&
		    /* M_TRAILINGSPACE() checks buffer writeability */
		    m->m_len <= MCLBYTES / 4 && /* XXX Don't copy too much */
		    m->m_len <= M_TRAILINGSPACE(n) &&
		    n->m_type == m->m_type) {
			memcpy(mtod(n, char *) + n->m_len, mtod(m, void *),
			    (unsigned)m->m_len);
			n->m_len += m->m_len;
			sb->sb_cc += m->m_len;
			m = m_free(m);
			continue;
		}
		if (n)
			n->m_next = m;
		else
			sb->sb_mb = m;
		sb->sb_mbtail = m;
		sballoc(sb, m);
		n = m;
		m->m_flags &= ~M_EOR;
		m = m->m_next;
		n->m_next = 0;
	}
	if (eor) {
		if (n)
			n->m_flags |= eor;
		else
			printf("semi-panic: sbcompress\n");
	}
	SBLASTMBUFCHK(sb, __func__);
}

/*
 * Free all mbufs in a sockbuf.
 * Check that all resources are reclaimed.
 */
void
sbflush(struct sockbuf *sb)
{

	KASSERT(solocked(sb->sb_so));
	KASSERT((sb->sb_flags & SB_LOCK) == 0);

	while (sb->sb_mbcnt)
		sbdrop(sb, (int)sb->sb_cc);

	KASSERT(sb->sb_cc == 0);
	KASSERT(sb->sb_mb == NULL);
	KASSERT(sb->sb_mbtail == NULL);
	KASSERT(sb->sb_lastrecord == NULL);
}

/*
 * Drop data from (the front of) a sockbuf.
 */
void
sbdrop(struct sockbuf *sb, int len)
{
	struct mbuf	*m, *mn, *next;

	KASSERT(solocked(sb->sb_so));

	next = (m = sb->sb_mb) ? m->m_nextpkt : NULL;
	while (len > 0) {
		if (m == NULL) {
			if (next == NULL)
				panic("sbdrop(%p,%d): cc=%lu",
				    sb, len, sb->sb_cc);
			m = next;
			next = m->m_nextpkt;
			continue;
		}
		if (m->m_len > len) {
			m->m_len -= len;
			m->m_data += len;
			sb->sb_cc -= len;
			break;
		}
		len -= m->m_len;
		sbfree(sb, m);
		MFREE(m, mn);
		m = mn;
	}
	while (m && m->m_len == 0) {
		sbfree(sb, m);
		MFREE(m, mn);
		m = mn;
	}
	if (m) {
		sb->sb_mb = m;
		m->m_nextpkt = next;
	} else
		sb->sb_mb = next;
	/*
	 * First part is an inline SB_EMPTY_FIXUP().  Second part
	 * makes sure sb_lastrecord is up-to-date if we dropped
	 * part of the last record.
	 */
	m = sb->sb_mb;
	if (m == NULL) {
		sb->sb_mbtail = NULL;
		sb->sb_lastrecord = NULL;
	} else if (m->m_nextpkt == NULL)
		sb->sb_lastrecord = m;
}

/*
 * Drop a record off the front of a sockbuf
 * and move the next record to the front.
 */
void
sbdroprecord(struct sockbuf *sb)
{
	struct mbuf	*m, *mn;

	KASSERT(solocked(sb->sb_so));

	m = sb->sb_mb;
	if (m) {
		sb->sb_mb = m->m_nextpkt;
		do {
			sbfree(sb, m);
			MFREE(m, mn);
		} while ((m = mn) != NULL);
	}
	SB_EMPTY_FIXUP(sb);
}

/*
 * Create a "control" mbuf containing the specified data
 * with the specified type for presentation on a socket buffer.
 */
struct mbuf *
sbcreatecontrol1(void **p, int size, int type, int level, int flags)
{
	struct cmsghdr	*cp;
	struct mbuf	*m;
	int space = CMSG_SPACE(size);

	if ((flags & M_DONTWAIT) && space > MCLBYTES) {
		printf("%s: message too large %d\n", __func__, space);
		return NULL;
	}

	if ((m = m_get(flags, MT_CONTROL)) == NULL)
		return NULL;
	if (space > MLEN) {
		if (space > MCLBYTES)
			MEXTMALLOC(m, space, M_WAITOK);
		else
			MCLGET(m, flags);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return NULL;
		}
	}
	cp = mtod(m, struct cmsghdr *);
	*p = CMSG_DATA(cp);
	m->m_len = space;
	cp->cmsg_len = CMSG_LEN(size);
	cp->cmsg_level = level;
	cp->cmsg_type = type;
	return m;
}

struct mbuf *
sbcreatecontrol(void *p, int size, int type, int level)
{
	struct mbuf *m;
	void *v;

	m = sbcreatecontrol1(&v, size, type, level, M_DONTWAIT);
	if (m == NULL)
		return NULL;
	memcpy(v, p, size);
	return m;
}

void
solockretry(struct socket *so, kmutex_t *lock)
{

	while (lock != so->so_lock) {
		mutex_exit(lock);
		lock = so->so_lock;
		mutex_enter(lock);
	}
}

bool
solocked(struct socket *so)
{

	return mutex_owned(so->so_lock);
}

bool
solocked2(struct socket *so1, struct socket *so2)
{
	kmutex_t *lock;

	lock = so1->so_lock;
	if (lock != so2->so_lock)
		return false;
	return mutex_owned(lock);
}

/*
 * sosetlock: assign a default lock to a new socket.
 */
void
sosetlock(struct socket *so)
{
	if (so->so_lock == NULL) {
		kmutex_t *lock = softnet_lock;

		so->so_lock = lock;
		mutex_obj_hold(lock);
		mutex_enter(lock);
	}
	KASSERT(solocked(so));
}

/*
 * Set lock on sockbuf sb; sleep if lock is already held.
 * Unless SB_NOINTR is set on sockbuf, sleep is interruptible.
 * Returns error without lock if sleep is interrupted.
 */
int
sblock(struct sockbuf *sb, int wf)
{
	struct socket *so;
	kmutex_t *lock;
	int error;

	KASSERT(solocked(sb->sb_so));

	for (;;) {
		if (__predict_true((sb->sb_flags & SB_LOCK) == 0)) {
			sb->sb_flags |= SB_LOCK;
			return 0;
		}
		if (wf != M_WAITOK)
			return EWOULDBLOCK;
		so = sb->sb_so;
		lock = so->so_lock;
		if ((sb->sb_flags & SB_NOINTR) != 0) {
			cv_wait(&so->so_cv, lock);
			error = 0;
		} else
			error = cv_wait_sig(&so->so_cv, lock);
		if (__predict_false(lock != so->so_lock))
			solockretry(so, lock);
		if (error != 0)
			return error;
	}
}

void
sbunlock(struct sockbuf *sb)
{
	struct socket *so;

	so = sb->sb_so;

	KASSERT(solocked(so));
	KASSERT((sb->sb_flags & SB_LOCK) != 0);

	sb->sb_flags &= ~SB_LOCK;
	cv_broadcast(&so->so_cv);
}

int
sowait(struct socket *so, bool catch_p, int timo)
{
	kmutex_t *lock;
	int error;

	KASSERT(solocked(so));
	KASSERT(catch_p || timo != 0);

	lock = so->so_lock;
	if (catch_p)
		error = cv_timedwait_sig(&so->so_cv, lock, timo);
	else
		error = cv_timedwait(&so->so_cv, lock, timo);
	if (__predict_false(lock != so->so_lock))
		solockretry(so, lock);
	return error;
}
