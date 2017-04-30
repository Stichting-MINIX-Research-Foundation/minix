/* Socket event dispatching library - by D.C. van Moolenbroek */

#include <minix/drivers.h>
#include <minix/sockdriver.h>
#include <minix/sockevent.h>
#include <sys/ioctl.h>

#include "sockevent_proc.h"

#define US		1000000UL	/* microseconds per second */

#define SOCKHASH_SLOTS	256		/* # slots in ID-to-sock hash table */

static SLIST_HEAD(, sock) sockhash[SOCKHASH_SLOTS];

static SLIST_HEAD(, sock) socktimer;

static minix_timer_t sockevent_timer;

static SIMPLEQ_HEAD(, sock) sockevent_pending;

static sockevent_socket_cb_t sockevent_socket_cb = NULL;

static int sockevent_working;

static void socktimer_del(struct sock * sock);
static void sockevent_cancel_send(struct sock * sock,
	struct sockevent_proc * spr, int err);
static void sockevent_cancel_recv(struct sock * sock,
	struct sockevent_proc * spr, int err);

/*
 * Initialize the hash table of sock objects.
 */
static void
sockhash_init(void)
{
	unsigned int slot;

	for (slot = 0; slot < __arraycount(sockhash); slot++)
		SLIST_INIT(&sockhash[slot]);
}

/*
 * Given a socket identifier, return a hash table slot number.
 */
static unsigned int
sockhash_slot(sockid_t id)
{

	/*
	 * The idea of the shift is that a socket driver may offer multiple
	 * classes of sockets, and put the class in the higher bits.  The shift
	 * aims to prevent that all classes' first sockets end up in the same
	 * hash slot.
	 */
	return (id + (id >> 16)) % SOCKHASH_SLOTS;
}

/*
 * Obtain a sock object from the hash table using its unique identifier.
 * Return a pointer to the object if found, or NULL otherwise.
 */
static struct sock *
sockhash_get(sockid_t id)
{
	struct sock *sock;
	unsigned int slot;

	slot = sockhash_slot(id);

	SLIST_FOREACH(sock, &sockhash[slot], sock_hash) {
		if (sock->sock_id == id)
			return sock;
	}

	return NULL;
}

/*
 * Add a sock object to the hash table.  The sock object must have a valid ID
 * in its 'sock_id' field, and must not be in the hash table already.
 */
static void
sockhash_add(struct sock * sock)
{
	unsigned int slot;

	slot = sockhash_slot(sock->sock_id);

	SLIST_INSERT_HEAD(&sockhash[slot], sock, sock_hash);
}

/*
 * Remove a sock object from the hash table.  The sock object must be in the
 * hash table.
 */
static void
sockhash_del(struct sock * sock)
{
	unsigned int slot;

	slot = sockhash_slot(sock->sock_id);

	/* This macro is O(n). */
	SLIST_REMOVE(&sockhash[slot], sock, sock, sock_hash);
}

/*
 * Reset a socket object to a proper initial state, with a particular socket
 * identifier, a SOCK_ type, and a socket operations table.  The socket is
 * added to the ID-to-object hash table.  This function always succeeds.
 */
static void
sockevent_reset(struct sock * sock, sockid_t id, int domain, int type,
	const struct sockevent_ops * ops)
{

	assert(sock != NULL);

	memset(sock, 0, sizeof(*sock));

	sock->sock_id = id;
	sock->sock_domain = domain;
	sock->sock_type = type;

	sock->sock_slowat = 1;
	sock->sock_rlowat = 1;

	sock->sock_ops = ops;
	sock->sock_proc = NULL;
	sock->sock_select.ss_endpt = NONE;

	sockhash_add(sock);
}

/*
 * Initialize a new socket that will serve as an accepted socket on the given
 * listening socket 'sock'.  The new socket is given as 'newsock', and its new
 * socket identifier is given as 'newid'.  This function always succeeds.
 */
void
sockevent_clone(struct sock * sock, struct sock * newsock, sockid_t newid)
{

	sockevent_reset(newsock, newid, (int)sock->sock_domain,
	    sock->sock_type, sock->sock_ops);

	/* These are the settings that are currently inherited. */
	newsock->sock_opt = sock->sock_opt & ~SO_ACCEPTCONN;
	newsock->sock_linger = sock->sock_linger;
	newsock->sock_stimeo = sock->sock_stimeo;
	newsock->sock_rtimeo = sock->sock_rtimeo;
	newsock->sock_slowat = sock->sock_slowat;
	newsock->sock_rlowat = sock->sock_rlowat;

	newsock->sock_flags |= SFL_CLONED;
}

/*
 * A new socket has just been accepted.  The corresponding listening socket is
 * given as 'sock'.  The new socket has ID 'newid', and if it had not already
 * been added to the hash table through sockevent_clone() before, 'newsock' is
 * a non-NULL pointer which identifies the socket object to clone into.
 */
static void
sockevent_accepted(struct sock * sock, struct sock * newsock, sockid_t newid)
{

	if (newsock == NULL) {
		if ((newsock = sockhash_get(newid)) == NULL)
			panic("libsockdriver: socket driver returned unknown "
			    "ID %d from accept callback", newid);
	} else
		sockevent_clone(sock, newsock, newid);

	assert(newsock->sock_flags & SFL_CLONED);
	newsock->sock_flags &= ~SFL_CLONED;
}

/*
 * Allocate a sock object, by asking the socket driver for one.  On success,
 * return OK, with a pointer to the new object stored in 'sockp'.  This new
 * object has all its fields set to initial values, in part based on the given
 * parameters.  On failure, return an error code.  Failure has two typical
 * cause: either the given domain, type, protocol combination is not supported,
 * or the socket driver is out of sockets (globally or for this combination).
 */
static int
sockevent_alloc(int domain, int type, int protocol, endpoint_t user_endpt,
	struct sock ** sockp)
{
	struct sock *sock;
	const struct sockevent_ops *ops;
	sockid_t r;

	/*
	 * Verify that the given domain is sane.  Unlike the type and protocol,
	 * the domain is already verified by VFS, so we do not limit ourselves
	 * here.  The result is that we can store the domain in just a byte.
	 */
	if (domain < 0 || domain > UINT8_MAX)
		return EAFNOSUPPORT;

	/* Make sure that the library has actually been initialized. */
	if (sockevent_socket_cb == NULL)
		panic("libsockevent: not initialized");

	sock = NULL;
	ops = NULL;

	/*
	 * Ask the socket driver to create a socket for the given combination
	 * of domain, type, and protocol.  If so, let it return a new sock
	 * object, a unique socket identifier for that object, and an
	 * operations table for it.
	 */
	if ((r = sockevent_socket_cb(domain, type, protocol, user_endpt, &sock,
	    &ops)) < 0)
		return r;

	assert(sock != NULL);
	assert(ops != NULL);

	sockevent_reset(sock, r, domain, type, ops);

	*sockp = sock;
	return OK;
}

/*
 * Free a previously allocated sock object.
 */
static void
sockevent_free(struct sock * sock)
{
	const struct sockevent_ops *ops;

	assert(sock->sock_proc == NULL);

	socktimer_del(sock);

	sockhash_del(sock);

	/*
	 * Invalidate the operations table on the socket, before freeing the
	 * socket.  This allows us to detect cases where sockevent functions
	 * are called on sockets that have already been freed.
	 */
	ops = sock->sock_ops;
	sock->sock_ops = NULL;

	assert(ops != NULL);
	assert(ops->sop_free != NULL);

	ops->sop_free(sock);
}

/*
 * Create a new socket.
 */
static sockid_t
sockevent_socket(int domain, int type, int protocol, endpoint_t user_endpt)
{
	struct sock *sock;
	int r;

	if ((r = sockevent_alloc(domain, type, protocol, user_endpt,
	    &sock)) != OK)
		return r;

	return sock->sock_id;
}

/*
 * Create a pair of connected sockets.
 */
static int
sockevent_socketpair(int domain, int type, int protocol, endpoint_t user_endpt,
	sockid_t id[2])
{
	struct sock *sock1, *sock2;
	int r;

	if ((r = sockevent_alloc(domain, type, protocol, user_endpt,
	    &sock1)) != OK)
		return r;

	/* Creating socket pairs is not always supported. */
	if (sock1->sock_ops->sop_pair == NULL) {
		sockevent_free(sock1);

		return EOPNOTSUPP;
	}

	if ((r = sockevent_alloc(domain, type, protocol, user_endpt,
	    &sock2)) != OK) {
		sockevent_free(sock1);

		return r;
	}

	assert(sock1->sock_ops == sock2->sock_ops);

	r = sock1->sock_ops->sop_pair(sock1, sock2, user_endpt);

	if (r != OK) {
		sockevent_free(sock2);
		sockevent_free(sock1);

		return r;
	}

	id[0] = sock1->sock_id;
	id[1] = sock2->sock_id;
	return OK;
}

/*
 * A send request returned EPIPE.  If desired, send a SIGPIPE signal to the
 * user process that issued the request.
 */
static void
sockevent_sigpipe(struct sock * sock, endpoint_t user_endpt, int flags)
{

	/*
	 * POSIX says that pipe signals should be generated for SOCK_STREAM
	 * sockets.  Linux does just this, NetBSD raises signals for all socket
	 * types.
	 */
	if (sock->sock_type != SOCK_STREAM)
		return;

	/*
	 * Why would there be fewer than four ways to do the same thing?
	 * O_NOSIGPIPE, MSG_NOSIGNAL, SO_NOSIGPIPE, and of course blocking
	 * SIGPIPE.  VFS already sets MSG_NOSIGNAL for calls on sockets with
	 * O_NOSIGPIPE.  The fact that SO_NOSIGPIPE is a thing, is also the
	 * reason why we cannot let VFS handle signal generation altogether.
	 */
	if (flags & MSG_NOSIGNAL)
		return;
	if (sock->sock_opt & SO_NOSIGPIPE)
		return;

	/*
	 * Send a SIGPIPE signal to the user process.  Unfortunately we cannot
	 * guarantee that the SIGPIPE reaches the user process before the send
	 * call returns.  Usually, the scheduling priorities of system services
	 * are such that the signal is likely to arrive first anyway, but if
	 * timely arrival of the signal is required, a more fundamental change
	 * to the system would be needed.
	 */
	sys_kill(user_endpt, SIGPIPE);
}

/*
 * Suspend a request without data, that is, a bind, connect, accept, or close
 * request.
 */
static void
sockevent_suspend(struct sock * sock, unsigned int event,
	const struct sockdriver_call * __restrict call, endpoint_t user_endpt)
{
	struct sockevent_proc *spr, **sprp;

	/* There is one slot for each process, so this should never fail. */
	if ((spr = sockevent_proc_alloc()) == NULL)
		panic("libsockevent: too many suspended processes");

	spr->spr_next = NULL;
	spr->spr_event = event;
	spr->spr_timer = FALSE;
	spr->spr_call = *call;
	spr->spr_endpt = user_endpt;

	/*
	 * Add the request to the tail of the queue.  This operation is O(n),
	 * but the number of suspended requests per socket is expected to be
	 * low at all times.
	 */
	for (sprp = &sock->sock_proc; *sprp != NULL;
	     sprp = &(*sprp)->spr_next);
	*sprp = spr;
}

/*
 * Suspend a request with data, that is, a send or receive request.
 */
static void
sockevent_suspend_data(struct sock * sock, unsigned int event, int timer,
	const struct sockdriver_call * __restrict call, endpoint_t user_endpt,
	const struct sockdriver_data * __restrict data, size_t len, size_t off,
	const struct sockdriver_data * __restrict ctl, socklen_t ctl_len,
	socklen_t ctl_off, int flags, int rflags, clock_t time)
{
	struct sockevent_proc *spr, **sprp;

	/* There is one slot for each process, so this should never fail. */
	if ((spr = sockevent_proc_alloc()) == NULL)
		panic("libsockevent: too many suspended processes");

	spr->spr_next = NULL;
	spr->spr_event = event;
	spr->spr_timer = timer;
	spr->spr_call = *call;
	spr->spr_endpt = user_endpt;
	sockdriver_pack_data(&spr->spr_data, call, data, len);
	spr->spr_datalen = len;
	spr->spr_dataoff = off;
	sockdriver_pack_data(&spr->spr_ctl, call, ctl, ctl_len);
	spr->spr_ctllen = ctl_len;
	spr->spr_ctloff = ctl_off;
	spr->spr_flags = flags;
	spr->spr_rflags = rflags;
	spr->spr_time = time;

	/*
	 * Add the request to the tail of the queue.  This operation is O(n),
	 * but the number of suspended requests per socket is expected to be
	 * low at all times.
	 */
	for (sprp = &sock->sock_proc; *sprp != NULL;
	     sprp = &(*sprp)->spr_next);
	*sprp = spr;
}

/*
 * Return TRUE if there are any suspended requests on the given socket's queue
 * that match any of the events in the given event mask, or FALSE otherwise.
 */
static int
sockevent_has_suspended(struct sock * sock, unsigned int mask)
{
	struct sockevent_proc *spr;

	for (spr = sock->sock_proc; spr != NULL; spr = spr->spr_next)
		if (spr->spr_event & mask)
			return TRUE;

	return FALSE;
}

/*
 * Check whether the given call is on the given socket's queue of suspended
 * requests.  If so, remove it from the queue and return a pointer to the
 * suspension data structure.  The caller is then responsible for freeing that
 * data structure using sockevent_proc_free().  If the call was not found, the
 * function returns NULL.
 */
static struct sockevent_proc *
sockevent_unsuspend(struct sock * sock, const struct sockdriver_call * call)
{
	struct sockevent_proc *spr, **sprp;

	/* Find the suspended request being canceled. */
	for (sprp = &sock->sock_proc; (spr = *sprp) != NULL;
	    sprp = &spr->spr_next) {
		if (spr->spr_call.sc_endpt == call->sc_endpt &&
		    spr->spr_call.sc_req == call->sc_req) {
			/* Found; remove and return it. */
			*sprp = spr->spr_next;

			return spr;
		}
	}

	return NULL;
}

/*
 * Attempt to resume the given suspended request for the given socket object.
 * Return TRUE if the suspended request has been fully resumed and can be
 * removed from the queue of suspended requests, or FALSE if it has not been
 * fully resumed and should stay on the queue.  In the latter case, no
 * resumption will be attempted for other suspended requests of the same type.
 */
static int
sockevent_resume(struct sock * sock, struct sockevent_proc * spr)
{
	struct sock *newsock;
	struct sockdriver_data data, ctl;
	char addr[SOCKADDR_MAX];
	socklen_t addr_len;
	size_t len, min;
	sockid_t r;

	switch (spr->spr_event) {
	case SEV_CONNECT:
		/*
		 * If the connect call was suspended for the purpose of
		 * intercepting resumption, simply remove it from the queue.
		 */
		if (spr->spr_call.sc_endpt == NONE)
			return TRUE;

		/* FALLTHROUGH */
	case SEV_BIND:
		if ((r = sock->sock_err) != OK)
			sock->sock_err = OK;

		sockdriver_reply_generic(&spr->spr_call, r);

		return TRUE;

	case SEV_ACCEPT:
		/*
		 * A previous accept call may not have blocked on a socket that
		 * was not in listening mode.
		 */
		assert(sock->sock_opt & SO_ACCEPTCONN);

		addr_len = 0;
		newsock = NULL;

		/*
		 * This call is suspended, which implies that the call table
		 * pointer has already tested to be non-NULL.
		 */
		if ((r = sock->sock_ops->sop_accept(sock,
		    (struct sockaddr *)&addr, &addr_len, spr->spr_endpt,
		    &newsock)) == SUSPEND)
			return FALSE;

		if (r >= 0) {
			assert(addr_len <= sizeof(addr));

			sockevent_accepted(sock, newsock, r);
		}

		sockdriver_reply_accept(&spr->spr_call, r,
		    (struct sockaddr *)&addr, addr_len);

		return TRUE;

	case SEV_SEND:
		if (sock->sock_err != OK || (sock->sock_flags & SFL_SHUT_WR)) {
			if (spr->spr_dataoff > 0 || spr->spr_ctloff > 0)
				r = (int)spr->spr_dataoff;
			else if ((r = sock->sock_err) != OK)
				sock->sock_err = OK;
			else
				r = EPIPE;
		} else {
			sockdriver_unpack_data(&data, &spr->spr_call,
			    &spr->spr_data, spr->spr_datalen);
			sockdriver_unpack_data(&ctl, &spr->spr_call,
			    &spr->spr_ctl, spr->spr_ctllen);

			len = spr->spr_datalen - spr->spr_dataoff;

			min = sock->sock_slowat;
			if (min > len)
				min = len;

			/*
			 * As mentioned elsewhere, we do not save the address
			 * upon suspension so we cannot supply it anymore here.
			 */
			r = sock->sock_ops->sop_send(sock, &data, len,
			    &spr->spr_dataoff, &ctl,
			    spr->spr_ctllen - spr->spr_ctloff,
			    &spr->spr_ctloff, NULL, 0, spr->spr_endpt,
			    spr->spr_flags, min);

			assert(r <= 0);

			if (r == SUSPEND)
				return FALSE;

			/*
			 * If an error occurred but some data were already
			 * sent, return the progress rather than the error.
			 * Note that if the socket driver detects an
			 * asynchronous error during the send, it itself must
			 * perform this check and call sockevent_set_error() as
			 * needed, to make sure the error does not get lost.
			 */
			if (spr->spr_dataoff > 0 || spr->spr_ctloff > 0)
				r = spr->spr_dataoff;
		}

		if (r == EPIPE)
			sockevent_sigpipe(sock, spr->spr_endpt,
			    spr->spr_flags);

		sockdriver_reply_generic(&spr->spr_call, r);

		return TRUE;

	case SEV_RECV:
		addr_len = 0;

		if (sock->sock_flags & SFL_SHUT_RD)
			r = SOCKEVENT_EOF;
		else {
			len = spr->spr_datalen - spr->spr_dataoff;

			if (sock->sock_err == OK) {
				min = sock->sock_rlowat;
				if (min > len)
					min = len;
			} else
				min = 0;

			sockdriver_unpack_data(&data, &spr->spr_call,
			    &spr->spr_data, spr->spr_datalen);
			sockdriver_unpack_data(&ctl, &spr->spr_call,
			    &spr->spr_ctl, spr->spr_ctllen);

			r = sock->sock_ops->sop_recv(sock, &data, len,
			    &spr->spr_dataoff, &ctl,
			    spr->spr_ctllen - spr->spr_ctloff,
			    &spr->spr_ctloff, (struct sockaddr *)&addr,
			    &addr_len, spr->spr_endpt, spr->spr_flags, min,
			    &spr->spr_rflags);

			/*
			 * If the call remains suspended but a socket error is
			 * pending, return the pending socket error instead.
			 */
			if (r == SUSPEND) {
				if (sock->sock_err == OK)
					return FALSE;

				r = SOCKEVENT_EOF;
			}

			assert(addr_len <= sizeof(addr));
		}

		/*
		 * If the receive call reported success, or if some data were
		 * already received, return the (partial) result.  Otherwise,
		 * return a pending error if any, or otherwise a regular error
		 * or 0 for EOF.
		 */
		if (r == OK || spr->spr_dataoff > 0 || spr->spr_ctloff > 0)
			r = (int)spr->spr_dataoff;
		else if (sock->sock_err != OK) {
			r = sock->sock_err;

			sock->sock_err = OK;
		} else if (r == SOCKEVENT_EOF)
			r = 0; /* EOF */

		sockdriver_reply_recv(&spr->spr_call, r, spr->spr_ctloff,
		    (struct sockaddr *)&addr, addr_len, spr->spr_rflags);

		return TRUE;

	case SEV_CLOSE:
		sockdriver_reply_generic(&spr->spr_call, OK);

		return TRUE;

	default:
		panic("libsockevent: process suspended on unknown event 0x%x",
		    spr->spr_event);
	}
}

/*
 * Return TRUE if the given socket is ready for reading for a select call, or
 * FALSE otherwise.
 */
static int
sockevent_test_readable(struct sock * sock)
{
	int r;

	/*
	 * The meaning of "ready-to-read" depends on whether the socket is a
	 * listening socket or not.  For the former, it is a test on whether
	 * there are any new sockets to accept.  However, shutdown flags take
	 * precedence in both cases.
	 */
	if (sock->sock_flags & SFL_SHUT_RD)
		return TRUE;

	if (sock->sock_err != OK)
		return TRUE;

	/*
	 * Depending on whether this is a listening-mode socket, test whether
	 * either accepts or receives would block.
	 */
	if (sock->sock_opt & SO_ACCEPTCONN) {
		if (sock->sock_ops->sop_test_accept == NULL)
			return TRUE;

		r = sock->sock_ops->sop_test_accept(sock);
	} else {
		if (sock->sock_ops->sop_test_recv == NULL)
			return TRUE;

		r = sock->sock_ops->sop_test_recv(sock, sock->sock_rlowat,
		    NULL);
	}

	return (r != SUSPEND);
}

/*
 * Return TRUE if the given socket is ready for writing for a select call, or
 * FALSE otherwise.
 */
static int
sockevent_test_writable(struct sock * sock)
{
	int r;

	if (sock->sock_err != OK)
		return TRUE;

	if (sock->sock_flags & SFL_SHUT_WR)
		return TRUE;

	if (sock->sock_ops->sop_test_send == NULL)
		return TRUE;

	/*
	 * Test whether sends would block.  The low send watermark is relevant
	 * for stream-type sockets only.
	 */
	r = sock->sock_ops->sop_test_send(sock, sock->sock_slowat);

	return (r != SUSPEND);
}

/*
 * Test whether any of the given select operations are ready on the given
 * socket.  Return the subset of ready operations; zero if none.
 */
static unsigned int
sockevent_test_select(struct sock * sock, unsigned int ops)
{
	unsigned int ready_ops;

	assert(!(ops & ~(SDEV_OP_RD | SDEV_OP_WR | SDEV_OP_ERR)));

	/*
	 * We do not support the "bind in progress" case here.  If a blocking
	 * bind call is in progress, the file descriptor should not be ready
	 * for either reading or writing.  Currently, socket drivers will have
	 * to cover this case themselves.  Otherwise we would have to check the
	 * queue of suspended calls, or create a custom flag for this.
	 */

	ready_ops = 0;

	if ((ops & SDEV_OP_RD) && sockevent_test_readable(sock))
		ready_ops |= SDEV_OP_RD;

	if ((ops & SDEV_OP_WR) && sockevent_test_writable(sock))
		ready_ops |= SDEV_OP_WR;

	/* TODO: OOB receive support. */

	return ready_ops;
}

/*
 * Fire the given mask of events on the given socket object now.
 */
static void
sockevent_fire(struct sock * sock, unsigned int mask)
{
	struct sockevent_proc *spr, **sprp;
	unsigned int r, flag, ops;

	/*
	 * A completed connection attempt (successful or not) also always
	 * implies that the socket becomes writable.  For convenience we
	 * enforce this rule here, because it is easy to forget.  Note that in
	 * any case, a suspended connect request should be the first in the
	 * list, so we do not risk returning 0 from a connect call as a result
	 * of sock_err getting eaten by another resumed call.
	 */
	if (mask & SEV_CONNECT)
		mask |= SEV_SEND;

	/*
	 * First try resuming regular system calls.
	 */
	for (sprp = &sock->sock_proc; (spr = *sprp) != NULL; ) {
		flag = spr->spr_event;

		if ((mask & flag) && sockevent_resume(sock, spr)) {
			*sprp = spr->spr_next;

			sockevent_proc_free(spr);
		} else {
			mask &= ~flag;

			sprp = &spr->spr_next;
		}
	}

	/*
	 * Then see if we can satisfy pending select queries.
	 */
	if ((mask & (SEV_ACCEPT | SEV_SEND | SEV_RECV)) &&
	    sock->sock_select.ss_endpt != NONE) {
		assert(sock->sock_selops != 0);

		/*
		 * Only retest select operations that, based on the given event
		 * mask, could possibly be satisfied now.
		 */
		ops = sock->sock_selops;
		if (!(mask & (SEV_ACCEPT | SEV_RECV)))
			ops &= ~SDEV_OP_RD;
		if (!(mask & SEV_SEND))
			ops &= ~SDEV_OP_WR;
		if (!(0))			/* TODO: OOB receive support */
			ops &= ~SDEV_OP_ERR;

		/* Are there any operations to test? */
		if (ops != 0) {
			/* Test those operations. */
			r = sockevent_test_select(sock, ops);

			/* Were any satisfied? */
			if (r != 0) {
				/* Let the caller know. */
				sockdriver_reply_select(&sock->sock_select,
				    sock->sock_id, r);

				sock->sock_selops &= ~r;

				/* Are there any saved operations left now? */
				if (sock->sock_selops == 0)
					sock->sock_select.ss_endpt = NONE;
			}
		}
	}

	/*
	 * Finally, a SEV_CLOSE event unconditionally frees the sock object.
	 * This event should be fired only for sockets that are either not yet,
	 * or not anymore, in use by userland.
	 */
	if (mask & SEV_CLOSE) {
		assert(sock->sock_flags & (SFL_CLONED | SFL_CLOSING));

		sockevent_free(sock);
	}
}

/*
 * Process all pending events.  Events must still be blocked, so that if
 * handling one event generates a new event, that event is handled from here
 * rather than immediately.
 */
static void
sockevent_pump(void)
{
	struct sock *sock;
	unsigned int mask;

	assert(sockevent_working);

	while (!SIMPLEQ_EMPTY(&sockevent_pending)) {
		sock = SIMPLEQ_FIRST(&sockevent_pending);
		SIMPLEQ_REMOVE_HEAD(&sockevent_pending, sock_next);

		mask = sock->sock_events;
		assert(mask != 0);
		sock->sock_events = 0;

		sockevent_fire(sock, mask);
		/*
		 * At this point, the sock object may already have been readded
		 * to the event list, or even be deallocated altogether.
		 */
	}
}

/*
 * Return TRUE if any events are pending on any sockets, or FALSE otherwise.
 */
static int
sockevent_has_events(void)
{

	return (!SIMPLEQ_EMPTY(&sockevent_pending));
}

/*
 * Raise the given bitwise-OR'ed set of events on the given socket object.
 * Depending on the context of the call, they events may or may not be
 * processed immediately.
 */
void
sockevent_raise(struct sock * sock, unsigned int mask)
{

	assert(sock->sock_ops != NULL);

	/*
	 * Handle SEV_CLOSE first.  This event must not be deferred, so as to
	 * let socket drivers recycle sock objects as they are needed.  For
	 * example, a user-closed TCP socket may stay open to transmit the
	 * remainder of its send buffer, until the TCP driver runs out of
	 * sockets, in which case the connection is aborted.  The driver would
	 * then raise SEV_CLOSE on the sock object so as to clean it up, and
	 * immediately reuse it afterward.  If the close event were to be
	 * deferred, this immediate reuse would not be possible.
	 *
	 * The sop_free() callback routine may not raise new events, and thus,
	 * the state of 'sockevent_working' need not be checked or set here.
	 */
	if (mask & SEV_CLOSE) {
		assert(mask == SEV_CLOSE);

		sockevent_fire(sock, mask);

		return;
	}

	/*
	 * If we are currently processing a socket message, store the event for
	 * later.  If not, this call is not coming from inside libsockevent,
	 * and we must handle the event immediately.
	 */
	if (sockevent_working) {
		assert(mask != 0);
		assert(mask <= UCHAR_MAX); /* sock_events field size check */

		if (sock->sock_events == 0)
			SIMPLEQ_INSERT_TAIL(&sockevent_pending, sock,
			    sock_next);

		sock->sock_events |= mask;
	} else {
		sockevent_working = TRUE;

		sockevent_fire(sock, mask);

		if (sockevent_has_events())
			sockevent_pump();

		sockevent_working = FALSE;
	}
}

/*
 * Set a pending error on the socket object, and wake up any suspended
 * operations that are affected by this.
 */
void
sockevent_set_error(struct sock * sock, int err)
{

	assert(err < 0);
	assert(sock->sock_ops != NULL);

	/* If an error was set already, it will be overridden. */
	sock->sock_err = err;

	sockevent_raise(sock, SEV_BIND | SEV_CONNECT | SEV_SEND | SEV_RECV);
}

/*
 * Initialize timer-related data structures.
 */
static void
socktimer_init(void)
{

	SLIST_INIT(&socktimer);

	init_timer(&sockevent_timer);
}

/*
 * Check whether the given socket object has any suspended requests that have
 * now expired.  If so, cancel them.  Also, if the socket object has any
 * suspended requests with a timeout that has not yet expired, return the
 * earliest (relative) timeout of all of them, or TMR_NEVER if no such requests
 * are present.
 */
static clock_t
sockevent_expire(struct sock * sock, clock_t now)
{
	struct sockevent_proc *spr, **sprp;
	clock_t lowest, left;
	int r;

	/*
	 * First handle the case that the socket is closed.  In this case,
	 * there may be a linger timer, although the socket may also simply
	 * still be on the timer list because of a request that did not time
	 * out right before the socket was closed.
	 */
	if (sock->sock_flags & SFL_CLOSING) {
		/* Was there a linger timer and has it expired? */
		if ((sock->sock_opt & SO_LINGER) &&
		    tmr_is_first(sock->sock_linger, now)) {
			assert(sock->sock_ops->sop_close != NULL);

			/*
			 * Whatever happens next, we must now resume the
			 * pending close operation, if it was not canceled
			 * earlier.  As before, we return OK rather than the
			 * standardized EWOULDBLOCK, to ensure that the user
			 * process knows the file descriptor has been closed.
			 */
			if ((spr = sock->sock_proc) != NULL) {
				assert(spr->spr_event == SEV_CLOSE);
				assert(spr->spr_next == NULL);

				sock->sock_proc = NULL;

				sockdriver_reply_generic(&spr->spr_call, OK);

				sockevent_proc_free(spr);
			}

			/*
			 * Tell the socket driver that closing the socket is
			 * now a bit more desired than the last time we asked.
			 */
			r = sock->sock_ops->sop_close(sock, TRUE /*force*/);

			assert(r == OK || r == SUSPEND);

			/*
			 * The linger timer fires once.  After that, the socket
			 * driver is free to decide that it still will not
			 * close the socket.  If it does, do not fire the
			 * linger timer again.
			 */
			if (r == SUSPEND)
				sock->sock_opt &= ~SO_LINGER;
			else
				sockevent_free(sock);
		}

		return TMR_NEVER;
	}

	/*
	 * Then see if any send and/or receive requests have expired.  Also see
	 * if there are any send and/or receive requests left that have not yet
	 * expired but do have a timeout, so that we can return the lowest of
	 * those timeouts.
	 */
	lowest = TMR_NEVER;

	for (sprp = &sock->sock_proc; (spr = *sprp) != NULL; ) {
		/* Skip requests without a timeout. */
		if (spr->spr_timer == 0) {
			sprp = &spr->spr_next;

			continue;
		}

		assert(spr->spr_event == SEV_SEND ||
		    spr->spr_event == SEV_RECV);

		/*
		 * If the request has expired, cancel it and remove it from the
		 * list.  Otherwise, see if the request has the lowest number
		 * of ticks until its timeout so far.
		 */
		if (tmr_is_first(spr->spr_time, now)) {
			*sprp = spr->spr_next;

			if (spr->spr_event == SEV_SEND)
				sockevent_cancel_send(sock, spr, EWOULDBLOCK);
			else
				sockevent_cancel_recv(sock, spr, EWOULDBLOCK);

			sockevent_proc_free(spr);
		} else {
			left = spr->spr_time - now;

			if (lowest == TMR_NEVER || lowest > left)
				lowest = left;

			sprp = &spr->spr_next;
		}
	}

	return lowest;
}

/*
 * The socket event alarm went off.  Go through the set of socket objects with
 * timers, and see if any of their requests have now expired.  Set a new alarm
 * as necessary.
 */
static void
socktimer_expire(int arg __unused)
{
	SLIST_HEAD(, sock) oldtimer;
	struct sock *sock, *tsock;
	clock_t now, lowest, left;
	int working;

	/*
	 * This function may or may not be called from a context where we are
	 * already deferring events, so we have to cover both cases here.
	 */
	if ((working = sockevent_working) == FALSE)
		sockevent_working = TRUE;

	/* Start a new list. */
	memcpy(&oldtimer, &socktimer, sizeof(oldtimer));
	SLIST_INIT(&socktimer);

	now = getticks();
	lowest = TMR_NEVER;

	/*
	 * Go through all sockets that have or had a request with a timeout,
	 * canceling any expired requests and building a new list of sockets
	 * that still have requests with timeouts as we go.
	 */
	SLIST_FOREACH_SAFE(sock, &oldtimer, sock_timer, tsock) {
		assert(sock->sock_flags & SFL_TIMER);
		sock->sock_flags &= ~SFL_TIMER;

		left = sockevent_expire(sock, now);
		/*
		 * The sock object may already have been deallocated now.
		 * If 'next' is TMR_NEVER, do not touch 'sock' anymore.
		 */

		if (left != TMR_NEVER) {
			if (lowest == TMR_NEVER || lowest > left)
				lowest = left;

			SLIST_INSERT_HEAD(&socktimer, sock, sock_timer);

			sock->sock_flags |= SFL_TIMER;
		}
	}

	/* If there is a new lowest timeout at all, set a new timer. */
	if (lowest != TMR_NEVER)
		set_timer(&sockevent_timer, lowest, socktimer_expire, 0);

	if (!working) {
		/* If any new events were raised, process them now. */
		if (sockevent_has_events())
			sockevent_pump();

		sockevent_working = FALSE;
	}
}

/*
 * Set a timer for the given (relative) number of clock ticks, adding the
 * associated socket object to the set of socket objects with timers, if it was
 * not already in that set.  Set a new alarm if necessary, and return the
 * absolute timeout for the timer.  Since the timers list is maintained lazily,
 * the caller need not take the object off the set if the call was canceled
 * later; see also socktimer_del().
 */
static clock_t
socktimer_add(struct sock * sock, clock_t ticks)
{
	clock_t now;

	/*
	 * Relative time comparisons require that any two times are no more
	 * than half the comparison space (clock_t, unsigned long) apart.
	 */
	assert(ticks <= TMRDIFF_MAX);

	/* If the socket was not already on the timers list, put it on. */
	if (!(sock->sock_flags & SFL_TIMER)) {
		SLIST_INSERT_HEAD(&socktimer, sock, sock_timer);

		sock->sock_flags |= SFL_TIMER;
	}

	/*
	 * (Re)set the timer if either it was not running at all or this new
	 * timeout will occur sooner than the currently scheduled alarm.  Note
	 * that setting a timer that was already set is allowed.
	 */
	now = getticks();

	if (!tmr_is_set(&sockevent_timer) ||
	    tmr_is_first(now + ticks, tmr_exp_time(&sockevent_timer)))
		set_timer(&sockevent_timer, ticks, socktimer_expire, 0);

	/* Return the absolute timeout. */
	return now + ticks;
}

/*
 * Remove a socket object from the set of socket objects with timers.  Since
 * the timer list is maintained lazily, this needs to be done only right before
 * the socket object is freed.
 */
static void
socktimer_del(struct sock * sock)
{

	if (sock->sock_flags & SFL_TIMER) {
		/* This macro is O(n). */
		SLIST_REMOVE(&socktimer, sock, sock, sock_timer);

		sock->sock_flags &= ~SFL_TIMER;
	}
}

/*
 * Bind a socket to a local address.
 */
static int
sockevent_bind(sockid_t id, const struct sockaddr * __restrict addr,
	socklen_t addr_len, endpoint_t user_endpt,
	const struct sockdriver_call * __restrict call)
{
	struct sock *sock;
	int r;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	if (sock->sock_ops->sop_bind == NULL)
		return EOPNOTSUPP;

	/* Binding a socket in listening mode is never supported. */
	if (sock->sock_opt & SO_ACCEPTCONN)
		return EINVAL;

	r = sock->sock_ops->sop_bind(sock, addr, addr_len, user_endpt);

	if (r == SUSPEND) {
		if (call == NULL)
			return EINPROGRESS;

		sockevent_suspend(sock, SEV_BIND, call, user_endpt);
	}

	return r;
}

/*
 * Connect a socket to a remote address.
 */
static int
sockevent_connect(sockid_t id, const struct sockaddr * __restrict addr,
	socklen_t addr_len, endpoint_t user_endpt,
	const struct sockdriver_call * call)
{
	struct sockdriver_call fakecall;
	struct sockevent_proc *spr;
	struct sock *sock;
	int r;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	if (sock->sock_ops->sop_connect == NULL)
		return EOPNOTSUPP;

	/* Connecting a socket in listening mode is never supported. */
	if (sock->sock_opt & SO_ACCEPTCONN)
		return EOPNOTSUPP;

	/*
	 * The upcoming connect call may fire an accept event for which the
	 * handler may in turn fire a connect event on this socket.  Since we
	 * delay event processing until after processing calls, this would
	 * create the problem that even if the connection is accepted right
	 * away, non-blocking connect requests would return EINPROGRESS.  For
	 * UDS, this is undesirable behavior.  To remedy this, we use a hack:
	 * we temporarily suspend the connect even if non-blocking, then
	 * process events, and then cancel the connect request again.  If the
	 * connection was accepted immediately, the cancellation will have no
	 * effect, since the request has already been replied to.  In order not
	 * to violate libsockdriver rules with this hack, we fabricate a fake
	 * 'conn' object.
	 */
	r = sock->sock_ops->sop_connect(sock, addr, addr_len, user_endpt);

	if (r == SUSPEND) {
		if (call != NULL || sockevent_has_events()) {
			if (call == NULL) {
				fakecall.sc_endpt = NONE;

				call = &fakecall;
			}

			assert(!sockevent_has_suspended(sock,
			    SEV_SEND | SEV_RECV));

			sockevent_suspend(sock, SEV_CONNECT, call, user_endpt);

			if (call == &fakecall) {
				/* Process any pending events first now. */
				sockevent_pump();

				/*
				 * If the connect request has not been resumed
				 * yet now, we must remove it from the queue
				 * again, and return EINPROGRESS ourselves.
				 * Otherwise, return OK or a pending error.
				 */
				spr = sockevent_unsuspend(sock, call);
				if (spr != NULL) {
					sockevent_proc_free(spr);

					r = EINPROGRESS;
				} else if ((r = sock->sock_err) != OK)
					sock->sock_err = OK;
			}
		} else
			r = EINPROGRESS;
	}

	if (r == OK) {
		/*
		 * A completed connection attempt also always implies that the
		 * socket becomes writable.  For convenience we enforce this
		 * rule here, because it is easy to forget.
		 */
		sockevent_raise(sock, SEV_SEND);
	}

	return r;
}

/*
 * Put a socket in listening mode.
 */
static int
sockevent_listen(sockid_t id, int backlog)
{
	struct sock *sock;
	int r;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	if (sock->sock_ops->sop_listen == NULL)
		return EOPNOTSUPP;

	/*
	 * Perform a general adjustment on the backlog value, applying the
	 * customary BSD "fudge factor" of 1.5x.  Keep the value within bounds
	 * though.  POSIX imposes that a negative backlog value is equal to a
	 * backlog value of zero.  A backlog value of zero, in turn, may mean
	 * anything; we take it to be one.  POSIX also imposes that all socket
	 * drivers accept up to at least SOMAXCONN connections on the queue.
	 */
	if (backlog < 0)
		backlog = 0;
	if (backlog < SOMAXCONN)
		backlog += 1 + ((unsigned int)backlog >> 1);
	if (backlog > SOMAXCONN)
		backlog = SOMAXCONN;

	r = sock->sock_ops->sop_listen(sock, backlog);

	/*
	 * On success, the socket is now in listening mode.  As part of that,
	 * a select(2) ready-to-read condition now indicates that a connection
	 * may be accepted on the socket, rather than that data may be read.
	 * Since libsockevent is responsible for this distinction, we keep
	 * track of the listening mode at this level.  Conveniently, there is a
	 * socket option for this, which we support out of the box as a result.
	 */
	if (r == OK) {
		sock->sock_opt |= SO_ACCEPTCONN;

		/*
		 * For the extremely unlikely case that right after the socket
		 * is put into listening mode, it has a connection ready to
		 * accept, we retest blocked ready-to-read select queries now.
		 */
		sockevent_raise(sock, SEV_ACCEPT);
	}

	return r;
}

/*
 * Accept a connection on a listening socket, creating a new socket.
 */
static sockid_t
sockevent_accept(sockid_t id, struct sockaddr * __restrict addr,
	socklen_t * __restrict addr_len, endpoint_t user_endpt,
	const struct sockdriver_call * __restrict call)
{
	struct sock *sock, *newsock;
	sockid_t r;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	if (sock->sock_ops->sop_accept == NULL)
		return EOPNOTSUPP;

	/*
	 * Attempt to accept a connection.  The socket driver is responsible
	 * for allocating a sock object (and identifier) on success.  It may
	 * already have done so before, in which case it should leave newsock
	 * filled with NULL; otherwise, the returned sock object is cloned from
	 * the listening socket.  The socket driver is also responsible for
	 * failing the call if the socket is not in listening mode, because it
	 * must specify the error to return: EOPNOTSUPP or EINVAL.
	 */
	newsock = NULL;

	if ((r = sock->sock_ops->sop_accept(sock, addr, addr_len, user_endpt,
	    &newsock)) == SUSPEND) {
		assert(sock->sock_opt & SO_ACCEPTCONN);

		if (call == NULL)
			return EWOULDBLOCK;

		sockevent_suspend(sock, SEV_ACCEPT, call, user_endpt);

		return SUSPEND;
	}

	if (r >= 0)
		sockevent_accepted(sock, newsock, r);

	return r;
}

/*
 * Send regular and/or control data.
 */
static int
sockevent_send(sockid_t id, const struct sockdriver_data * __restrict data,
	size_t len, const struct sockdriver_data * __restrict ctl_data,
	socklen_t ctl_len, const struct sockaddr * __restrict addr,
	socklen_t addr_len, endpoint_t user_endpt, int flags,
	const struct sockdriver_call * __restrict call)
{
	struct sock *sock;
	clock_t time;
	size_t min, off;
	socklen_t ctl_off;
	int r, timer;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	/*
	 * The order of the following checks is not necessarily fixed, and may
	 * be changed later.  As far as applicable, they should match the order
	 * of the checks during call resumption, though.
	 */
	if ((r = sock->sock_err) != OK) {
		sock->sock_err = OK;

		return r;
	}

	if (sock->sock_flags & SFL_SHUT_WR) {
		sockevent_sigpipe(sock, user_endpt, flags);

		return EPIPE;
	}

	/*
	 * Translate the sticky SO_DONTROUTE option to a per-request
	 * MSG_DONTROUTE flag.  This achieves two purposes: socket drivers have
	 * to check only one flag, and socket drivers that do not support the
	 * flag will fail send requests in a consistent way.
	 */
	if (sock->sock_opt & SO_DONTROUTE)
		flags |= MSG_DONTROUTE;

	/*
	 * Check if this is a valid send request as far as the socket driver is
	 * concerned.  We do this separately from sop_send for the reason that
	 * this send request may immediately be queued behind other pending
	 * send requests (without a call to sop_send), which means even invalid
	 * requests would be queued and not return failure until much later.
	 */
	if (sock->sock_ops->sop_pre_send != NULL &&
	    (r = sock->sock_ops->sop_pre_send(sock, len, ctl_len, addr,
	    addr_len, user_endpt,
	    flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL))) != OK)
		return r;

	if (sock->sock_ops->sop_send == NULL)
		return EOPNOTSUPP;

	off = 0;
	ctl_off = 0;

	/*
	 * Sending out-of-band data is treated differently from regular data:
	 *
	 * - sop_send is called immediately, even if a partial non-OOB send
	 *   operation is currently suspended (TODO: it may have to be aborted
	 *   in order to maintain atomicity guarantees - that should be easy);
	 * - sop_send must not return SUSPEND; instead, if it cannot process
	 *   the OOB data immediately, it must return an appropriate error;
	 * - the send low watermark is ignored.
	 *
	 * Given that none of the current socket drivers support OOB data at
	 * all, more sophisticated approaches would have no added value now.
	 */
	if (flags & MSG_OOB) {
		r = sock->sock_ops->sop_send(sock, data, len, &off, ctl_data,
		    ctl_len, &ctl_off, addr, addr_len, user_endpt, flags, 0);

		if (r == SUSPEND)
			panic("libsockevent: MSG_OOB send calls may not be "
			    "suspended");

		return (r == OK) ? (int)off : r;
	}

	/*
	 * Only call the actual sop_send function now if no other send calls
	 * are suspended already.
	 *
	 * Call sop_send with 'min' set to the minimum of the request size and
	 * the socket's send low water mark, but only if the call is non-
	 * blocking.  For stream-oriented sockets, this should have the effect
	 * that non-blocking calls fail with EWOULDBLOCK if not at least that
	 * much can be sent immediately. For consistency, we choose to apply
	 * the same threshold to blocking calls.  For datagram-oriented
	 * sockets, the minimum is not a factor to be considered.
	 */
	if (!sockevent_has_suspended(sock, SEV_SEND)) {
		min = sock->sock_slowat;
		if (min > len)
			min = len;

		r = sock->sock_ops->sop_send(sock, data, len, &off, ctl_data,
		    ctl_len, &ctl_off, addr, addr_len, user_endpt, flags, min);
	} else
		r = SUSPEND;

	if (r == SUSPEND) {
		/*
		 * We do not store the target's address on suspension, because
		 * that would add significantly to the per-process suspension
		 * state.  As a result, we disallow socket drivers from
		 * suspending send calls with addresses, because we would no
		 * longer have the address for proper call resumption.
		 * However, we do not know here whether the socket is in
		 * connection-oriented mode; if it is, the address is to be
		 * ignored altogether.  Therefore, there is no test on 'addr'
		 * here.  Resumed calls will get a NULL address pointer, and
		 * the socket driver is expected to do the right thing.
		 */

		/*
		 * For non-blocking socket calls, return an error only if we
		 * were not able to send anything at all.  If only control data
		 * were sent, the return value is therefore zero.
		 */
		if (call != NULL) {
			if (sock->sock_stimeo != 0) {
				timer = TRUE;
				time = socktimer_add(sock, sock->sock_stimeo);
			} else {
				timer = FALSE;
				time = 0;
			}

			sockevent_suspend_data(sock, SEV_SEND, timer, call,
			    user_endpt, data, len, off, ctl_data, ctl_len,
			    ctl_off, flags, 0, time);
		} else
			r = (off > 0 || ctl_off > 0) ? OK : EWOULDBLOCK;
	} else if (r == EPIPE)
		sockevent_sigpipe(sock, user_endpt, flags);

	return (r == OK) ? (int)off : r;
}

/*
 * The inner part of the receive request handler.  An error returned from here
 * may be overridden by an error pending on the socket, although data returned
 * from here trumps such pending errors.
 */
static int
sockevent_recv_inner(struct sock * sock,
	const struct sockdriver_data * __restrict data,
	size_t len, size_t * __restrict off,
	const struct sockdriver_data * __restrict ctl_data,
	socklen_t ctl_len, socklen_t * __restrict ctl_off,
	struct sockaddr * __restrict addr,
	socklen_t * __restrict addr_len, endpoint_t user_endpt,
	int * __restrict flags, const struct sockdriver_call * __restrict call)
{
	clock_t time;
	size_t min;
	int r, oob, inflags, timer;

	/*
	 * Check if this is a valid receive request as far as the socket driver
	 * is concerned.  We do this separately from sop_recv for the reason
	 * that this receive request may immediately be queued behind other
	 * pending receive requests (without a call to sop_recv), which means
	 * even invalid requests would be queued and not return failure until
	 * much later.
	 */
	inflags = *flags;
	*flags = 0;

	if (sock->sock_ops->sop_pre_recv != NULL &&
	    (r = sock->sock_ops->sop_pre_recv(sock, user_endpt,
	    inflags & ~(MSG_DONTWAIT | MSG_NOSIGNAL))) != OK)
		return r;

	/*
	 * The order of the following checks is not necessarily fixed, and may
	 * be changed later.  As far as applicable, they should match the order
	 * of the checks during call resumption, though.
	 */
	if (sock->sock_flags & SFL_SHUT_RD)
		return SOCKEVENT_EOF;

	if (sock->sock_ops->sop_recv == NULL)
		return EOPNOTSUPP;

	/*
	 * Receiving out-of-band data is treated differently from regular data:
	 *
	 * - sop_recv is called immediately, even if a partial non-OOB receive
	 *   operation is currently suspended (TODO: it may have to be aborted
	 *   in order to maintain atomicity guarantees - that should be easy);
	 * - sop_recv must not return SUSPEND; instead, if it cannot return any
	 *   the OOB data immediately, it must return an appropriate error;
	 * - the receive low watermark is ignored.
	 *
	 * Given that none of the current socket drivers support OOB data at
	 * all, more sophisticated approaches would have no added value now.
	 */
	oob = (inflags & MSG_OOB);

	if (oob && (sock->sock_opt & SO_OOBINLINE))
		return EINVAL;

	/*
	 * Only call the actual sop_recv function now if no other receive
	 * calls are suspended already.
	 *
	 * Call sop_recv with 'min' set to the minimum of the request size and
	 * the socket's socket's low water mark, unless there is a pending
	 * error.  As a result, blocking calls will block, and non-blocking
	 * calls will yield EWOULDBLOCK, if at least that much can be received,
	 * unless another condition (EOF or that pending error) prevents more
	 * from being received anyway.  For datagram-oriented sockets, the
	 * minimum is not a factor to be considered.
	 */
	if (oob || !sockevent_has_suspended(sock, SEV_RECV)) {
		if (!oob && sock->sock_err == OK) {
			min = sock->sock_rlowat;
			if (min > len)
				min = len;
		} else
			min = 0; /* receive even no-data segments */

		r = sock->sock_ops->sop_recv(sock, data, len, off, ctl_data,
		    ctl_len, ctl_off, addr, addr_len, user_endpt, inflags, min,
		    flags);
	} else
		r = SUSPEND;

	assert(r <= 0 || r == SOCKEVENT_EOF);

	if (r == SUSPEND) {
		if (oob)
			panic("libsockevent: MSG_OOB receive calls may not be "
			    "suspended");

		/*
		 * For non-blocking socket calls, return EWOULDBLOCK only if we
		 * did not receive anything at all.  If only control data were
		 * received, the return value is therefore zero.  Suspension
		 * implies that there is nothing to read.  For the purpose of
		 * the calling wrapper function, never suspend a call when
		 * there is a pending error.
		 */
		if (call != NULL && sock->sock_err == OK) {
			if (sock->sock_rtimeo != 0) {
				timer = TRUE;
				time = socktimer_add(sock, sock->sock_rtimeo);
			} else {
				timer = FALSE;
				time = 0;
			}

			sockevent_suspend_data(sock, SEV_RECV, timer, call,
			    user_endpt, data, len, *off, ctl_data,
			    ctl_len, *ctl_off, inflags, *flags, time);
		} else
			r = EWOULDBLOCK;
	}

	return r;
}

/*
 * Receive regular and/or control data.
 */
static int
sockevent_recv(sockid_t id, const struct sockdriver_data * __restrict data,
	size_t len, const struct sockdriver_data * __restrict ctl_data,
	socklen_t * __restrict ctl_len, struct sockaddr * __restrict addr,
	socklen_t * __restrict addr_len, endpoint_t user_endpt,
	int * __restrict flags, const struct sockdriver_call * __restrict call)
{
	struct sock *sock;
	size_t off;
	socklen_t ctl_inlen;
	int r;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	/*
	 * This function is a wrapper around the actual receive functionality.
	 * The reason for this is that receiving data should take precedence
	 * over a pending socket error, while a pending socket error should
	 * take precedence over both regular errors as well as EOF.  In other
	 * words: if there is a pending error, we must try to receive anything
	 * at all; if receiving does not work, we must fail the call with the
	 * pending error.  However, until we call the receive callback, we have
	 * no way of telling whether any data can be received.  So we must try
	 * that before we can decide whether to return a pending error.
	 */
	off = 0;
	ctl_inlen = *ctl_len;
	*ctl_len = 0;

	/*
	 * Attempt to perform the actual receive call.
	 */
	r = sockevent_recv_inner(sock, data, len, &off, ctl_data, ctl_inlen,
	    ctl_len, addr, addr_len, user_endpt, flags, call);

	/*
	 * If the receive request succeeded, or it failed but yielded a partial
	 * result, then return the (partal) result.  Otherwise, if an error is
	 * pending, return that error.  Otherwise, return either a regular
	 * error or 0 for EOF.
	 */
	if (r == OK || (r != SUSPEND && (off > 0 || *ctl_len > 0)))
		r = (int)off;
	else if (sock->sock_err != OK) {
		assert(r != SUSPEND);

		r = sock->sock_err;

		sock->sock_err = OK;
	} else if (r == SOCKEVENT_EOF)
		r = 0;

	return r;
}

/*
 * Process an I/O control call.
 */
static int
sockevent_ioctl(sockid_t id, unsigned long request,
	const struct sockdriver_data * __restrict data, endpoint_t user_endpt,
	const struct sockdriver_call * __restrict call __unused)
{
	struct sock *sock;
	size_t size;
	int r, val;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	/* We handle a very small subset of generic IOCTLs here. */
	switch (request) {
	case FIONREAD:
		size = 0;
		if (!(sock->sock_flags & SFL_SHUT_RD) &&
		    sock->sock_ops->sop_test_recv != NULL)
			(void)sock->sock_ops->sop_test_recv(sock, 0, &size);

		val = (int)size;

		return sockdriver_copyout(data, 0, &val, sizeof(val));
	}

	if (sock->sock_ops->sop_ioctl == NULL)
		return ENOTTY;

	r = sock->sock_ops->sop_ioctl(sock, request, data, user_endpt);

	/*
	 * Suspending IOCTL requests is not currently supported by this
	 * library, even though the VFS protocol and libsockdriver do support
	 * it.  The reason is that IOCTLs do not match our proces suspension
	 * model: they could be neither queued nor repeated.  For now, it seems
	 * that this feature is not needed by the socket drivers either.  Thus,
	 * even though there are possible solutions, we defer implementing them
	 * until we know what exactly is needed.
	 */
	if (r == SUSPEND)
		panic("libsockevent: socket driver suspended IOCTL 0x%lx",
		    request);

	return r;
}

/*
 * Set socket options.
 */
static int
sockevent_setsockopt(sockid_t id, int level, int name,
	const struct sockdriver_data * data, socklen_t len)
{
	struct sock *sock;
	struct linger linger;
	struct timeval tv;
	clock_t secs, ticks;
	int r, val;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	if (level == SOL_SOCKET) {
		/*
		 * Handle a subset of the socket-level options here.  For most
		 * of them, this means that the socket driver itself need not
		 * handle changing or returning the options, but still needs to
		 * implement the correct behavior based on them where needed.
		 * A few of them are handled exclusively in this library:
		 * SO_ACCEPTCONN, SO_NOSIGPIPE, SO_ERROR, SO_TYPE, SO_LINGER,
		 * SO_SNDLOWAT, SO_RCVLOWAT, SO_SNDTIMEO, and SO_RCVTIMEO.
		 * The SO_USELOOPBACK option is explicitly absent, as it is
		 * valid for routing sockets only and is set by default there.
		 */
		switch (name) {
		case SO_DEBUG:
		case SO_REUSEADDR:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_BROADCAST:
		case SO_OOBINLINE:
		case SO_REUSEPORT:
		case SO_NOSIGPIPE:
		case SO_TIMESTAMP:
			/*
			 * Simple on-off options.  Changing them does not
			 * involve the socket driver.
			 */
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val)
				sock->sock_opt |= (unsigned int)name;
			else
				sock->sock_opt &= ~(unsigned int)name;

			/*
			 * In priciple these on-off options are maintained in
			 * this library, but some socket drivers may need to
			 * apply the options elsewhere, so we notify them that
			 * something has changed.  Using the sop_setsockopt
			 * callback would be inconvenient for this for two
			 * reasons: multiple value copy-ins and default errors.
			 */
			if (sock->sock_ops->sop_setsockmask != NULL)
				sock->sock_ops->sop_setsockmask(sock,
				    sock->sock_opt);

			/*
			 * The inlining of OOB data may make new data available
			 * through regular receive calls.  Thus, see if we can
			 * wake up any suspended receive calls now.
			 */
			if (name == SO_OOBINLINE && val)
				sockevent_raise(sock, SEV_RECV);

			return OK;

		case SO_LINGER:
			/* The only on-off option with an associated value. */
			if ((r = sockdriver_copyin_opt(data, &linger,
			    sizeof(linger), len)) != OK)
				return r;

			if (linger.l_onoff) {
				if (linger.l_linger < 0)
					return EINVAL;
				/* EDOM is the closest applicable error.. */
				secs = (clock_t)linger.l_linger;
				if (secs >= TMRDIFF_MAX / sys_hz())
					return EDOM;

				sock->sock_opt |= SO_LINGER;
				sock->sock_linger = secs * sys_hz();
			} else {
				sock->sock_opt &= ~SO_LINGER;
				sock->sock_linger = 0;
			}

			return OK;

		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val <= 0)
				return EINVAL;

			/*
			 * Setting these values may allow suspended operations
			 * (send, recv, select) to be resumed, so recheck.
			 */
			if (name == SO_SNDLOWAT) {
				sock->sock_slowat = (size_t)val;

				sockevent_raise(sock, SEV_SEND);
			} else {
				sock->sock_rlowat = (size_t)val;

				sockevent_raise(sock, SEV_RECV);
			}

			return OK;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
			if ((r = sockdriver_copyin_opt(data, &tv, sizeof(tv),
			    len)) != OK)
				return r;

			if (tv.tv_sec < 0 || tv.tv_usec < 0 ||
			    (unsigned long)tv.tv_usec >= US)
				return EINVAL;
			if (tv.tv_sec >= TMRDIFF_MAX / sys_hz())
				return EDOM;

			ticks = tv.tv_sec * sys_hz() +
			    (tv.tv_usec * sys_hz() + US - 1) / US;

			if (name == SO_SNDTIMEO)
				sock->sock_stimeo = ticks;
			else
				sock->sock_rtimeo = ticks;

			/*
			 * The timeouts for any calls already in progress for
			 * this socket are left as is.
			 */
			return OK;

		case SO_ACCEPTCONN:
		case SO_ERROR:
		case SO_TYPE:
			/* These options may be retrieved but not set. */
			return ENOPROTOOPT;

		default:
			/*
			 * The remaining options either cannot be handled in a
			 * generic way, or are not recognized altogether.  Pass
			 * them to the socket driver, which should handle what
			 * it knows and reject the rest.
			 */
			break;
		}
	}

	if (sock->sock_ops->sop_setsockopt == NULL)
		return ENOPROTOOPT;

	/*
	 * The socket driver must return ENOPROTOOPT for all options it does
	 * not recognize.
	 */
	return sock->sock_ops->sop_setsockopt(sock, level, name, data, len);
}

/*
 * Retrieve socket options.
 */
static int
sockevent_getsockopt(sockid_t id, int level, int name,
	const struct sockdriver_data * __restrict data,
	socklen_t * __restrict len)
{
	struct sock *sock;
	struct linger linger;
	struct timeval tv;
	clock_t ticks;
	int val;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	if (level == SOL_SOCKET) {
		/*
		 * As with setting, handle a subset of the socket-level options
		 * here.  The rest is to be taken care of by the socket driver.
		 */
		switch (name) {
		case SO_DEBUG:
		case SO_ACCEPTCONN:
		case SO_REUSEADDR:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_BROADCAST:
		case SO_OOBINLINE:
		case SO_REUSEPORT:
		case SO_NOSIGPIPE:
		case SO_TIMESTAMP:
			val = !!(sock->sock_opt & (unsigned int)name);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case SO_LINGER:
			linger.l_onoff = !!(sock->sock_opt & SO_LINGER);
			linger.l_linger = sock->sock_linger / sys_hz();

			return sockdriver_copyout_opt(data, &linger,
			   sizeof(linger), len);

		case SO_ERROR:
			if ((val = -sock->sock_err) != OK)
				sock->sock_err = OK;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case SO_TYPE:
			val = sock->sock_type;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case SO_SNDLOWAT:
			val = (int)sock->sock_slowat;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case SO_RCVLOWAT:
			val = (int)sock->sock_rlowat;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
			if (name == SO_SNDTIMEO)
				ticks = sock->sock_stimeo;
			else
				ticks = sock->sock_rtimeo;

			tv.tv_sec = ticks / sys_hz();
			tv.tv_usec = (ticks % sys_hz()) * US / sys_hz();

			return sockdriver_copyout_opt(data, &tv, sizeof(tv),
			    len);

		default:
			break;
		}
	}

	if (sock->sock_ops->sop_getsockopt == NULL)
		return ENOPROTOOPT;

	/*
	 * The socket driver must return ENOPROTOOPT for all options it does
	 * not recognize.
	 */
	return sock->sock_ops->sop_getsockopt(sock, level, name, data, len);
}

/*
 * Retrieve a socket's local address.
 */
static int
sockevent_getsockname(sockid_t id, struct sockaddr * __restrict addr,
	socklen_t * __restrict addr_len)
{
	struct sock *sock;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	if (sock->sock_ops->sop_getsockname == NULL)
		return EOPNOTSUPP;

	return sock->sock_ops->sop_getsockname(sock, addr, addr_len);
}

/*
 * Retrieve a socket's remote address.
 */
static int
sockevent_getpeername(sockid_t id, struct sockaddr * __restrict addr,
	socklen_t * __restrict addr_len)
{
	struct sock *sock;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	/* Listening-mode sockets cannot possibly have a peer address. */
	if (sock->sock_opt & SO_ACCEPTCONN)
		return ENOTCONN;

	if (sock->sock_ops->sop_getpeername == NULL)
		return EOPNOTSUPP;

	return sock->sock_ops->sop_getpeername(sock, addr, addr_len);
}

/*
 * Mark the socket object as shut down for sending and/or receiving.  The flags
 * parameter may be a bitwise-OR'ed combination of SFL_SHUT_RD and SFL_SHUT_WR.
 * This function will wake up any suspended requests affected by this change,
 * but it will not invoke the sop_shutdown() callback function on the socket.
 * The function may in fact be called from sop_shutdown() before completion to
 * mark the socket as shut down as reflected by sockevent_is_shutdown().
 */
void
sockevent_set_shutdown(struct sock * sock, unsigned int flags)
{
	unsigned int mask;

	assert(sock->sock_ops != NULL);
	assert(!(flags & ~(SFL_SHUT_RD | SFL_SHUT_WR)));

	/* Look at the newly set flags only. */
	flags &= ~(unsigned int)sock->sock_flags;

	if (flags != 0) {
		sock->sock_flags |= flags;

		/*
		 * Wake up any blocked calls that are affected by the shutdown.
		 * Shutting down listening sockets causes ongoing accept calls
		 * to be rechecked.
		 */
		mask = 0;
		if (flags & SFL_SHUT_RD)
			mask |= SEV_RECV;
		if (flags & SFL_SHUT_WR)
			mask |= SEV_SEND;
		if (sock->sock_opt & SO_ACCEPTCONN)
			mask |= SEV_ACCEPT;

		assert(mask != 0);
		sockevent_raise(sock, mask);
	}
}

/*
 * Shut down socket send and receive operations.
 */
static int
sockevent_shutdown(sockid_t id, int how)
{
	struct sock *sock;
	unsigned int flags;
	int r;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	/* Convert the request to a set of flags. */
	flags = 0;
	if (how == SHUT_RD || how == SHUT_RDWR)
		flags |= SFL_SHUT_RD;
	if (how == SHUT_WR || how == SHUT_RDWR)
		flags |= SFL_SHUT_WR;

	if (sock->sock_ops->sop_shutdown != NULL)
		r = sock->sock_ops->sop_shutdown(sock, flags);
	else
		r = OK;

	/* On success, update our internal state as well. */
	if (r == OK)
		sockevent_set_shutdown(sock, flags);

	return r;
}

/*
 * Close a socket.
 */
static int
sockevent_close(sockid_t id, const struct sockdriver_call * call)
{
	struct sock *sock;
	int r, force;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	assert(sock->sock_proc == NULL);
	sock->sock_select.ss_endpt = NONE;

	/*
	 * There are several scenarios when it comes to closing sockets.  First
	 * of all, we never actually force the socket driver to close a socket.
	 * The driver may always suspend the close call and take as long as it
	 * wants.  After a suspension, it signals its completion of the close
	 * through the SEV_CLOSE socket event.
	 *
	 * With that said, we offer two levels of urgency regarding the close
	 * request: regular and forced.  The former allows for a graceful
	 * close; the latter urges the socket driver to close the socket as
	 * soon as possible.  A socket that has been requested to be closed
	 * gracefully can, as long as it is still open (i.e., no SEV_CLOSE was
	 * fired yet), later be requested to be closed forcefully.  This is how
	 * SO_LINGER with a nonzero timeout is implemented.  If SO_LINGER is
	 * set with a zero timeout, the socket is force-closed immediately.
	 * Finally, if SO_LINGER is not set, the socket will be closed normally
	 * and never be forced--akin to SO_LINGER with an infinite timeout.
	 *
	 * The return value of the caller's close(2) may only ever be either
	 * OK or EINPROGRESS, to ensure that the caller knows that the file
	 * descriptor is freed up, as per Austin Group Defect #529.  In fact,
	 * EINPROGRESS is to be returned only on signal interruption (i.e.,
	 * cancel).  For that reason, this function only ever returns OK.
	 */
	force = ((sock->sock_opt & SO_LINGER) && sock->sock_linger == 0);

	if (sock->sock_ops->sop_close != NULL)
		r = sock->sock_ops->sop_close(sock, force);
	else
		r = OK;

	assert(r == OK || r == SUSPEND);

	if (r == SUSPEND) {
		sock->sock_flags |= SFL_CLOSING;

		/*
		 * If we were requested to force-close the socket immediately,
		 * but the socket driver needs more time anyway, then tell the
		 * caller that the socket was closed right away.
		 */
		if (force)
			return OK;

		/*
		 * If we are to force-close the socket only after a specific
		 * linger timeout, set the timer for that now, even if the call
		 * is non-blocking.  This also means that we cannot associate
		 * the linger timeout with the close call.  Instead, we convert
		 * the sock_linger value from a (relative) duration to an
		 * (absolute) timeout time, and use the SFL_CLOSING flag (along
		 * with SFL_TIMER) to tell the difference.  Since the socket is
		 * otherwise unreachable from userland at this point, the
		 * conversion is never visible in any way.
		 *
		 * The socket may already be in the timers list, so we must
		 * always check the SO_LINGER flag before checking sock_linger.
		 *
		 * If SO_LINGER is not set, we must never suspend the call.
		 */
		if (sock->sock_opt & SO_LINGER) {
			sock->sock_linger =
			    socktimer_add(sock, sock->sock_linger);
		} else
			call = NULL;

		/*
		 * A non-blocking close is completed asynchronously.  The
		 * caller is not told about this with EWOULDBLOCK as usual, for
		 * the reasons mentioned above.
		 */
		if (call != NULL)
			sockevent_suspend(sock, SEV_CLOSE, call, NONE);
		else
			r = OK;
	} else if (r == OK)
		sockevent_free(sock);

	return r;
}

/*
 * Cancel a suspended send request.
 */
static void
sockevent_cancel_send(struct sock * sock, struct sockevent_proc * spr, int err)
{
	int r;

	/*
	 * If any regular or control data were sent, return the number of data
	 * bytes sent--possibly zero.  Otherwise return the given error code.
	 */
	if (spr->spr_dataoff > 0 || spr->spr_ctloff > 0)
		r = (int)spr->spr_dataoff;
	else
		r = err;

	sockdriver_reply_generic(&spr->spr_call, r);

	/*
	 * In extremely rare circumstances, one send may be queued behind
	 * another send even though the former can actually be sent on the
	 * socket right away.  For this reason, we retry sending when canceling
	 * a send.  We need to do this only when the first send in the queue
	 * was canceled, but multiple blocked sends on a single socket should
	 * be rare anyway.
	 */
	sockevent_raise(sock, SEV_SEND);
}

/*
 * Cancel a suspended receive request.
 */
static void
sockevent_cancel_recv(struct sock * sock, struct sockevent_proc * spr, int err)
{
	int r;

	/*
	 * If any regular or control data were received, return the number of
	 * data bytes received--possibly zero.  Otherwise return the given
	 * error code.
	 */
	if (spr->spr_dataoff > 0 || spr->spr_ctloff > 0)
		r = (int)spr->spr_dataoff;
	else
		r = err;

	/*
	 * Also return any flags set for the data received so far, e.g.
	 * MSG_CTRUNC.  Do not return an address: receive calls on unconnected
	 * sockets must never block after receiving some data--instead, they
	 * are supposed to return MSG_TRUNC if not all data were copied out.
	 */
	sockdriver_reply_recv(&spr->spr_call, r, spr->spr_ctloff, NULL, 0,
	    spr->spr_rflags);

	/*
	 * The same story as for sends (see above) applies to receives,
	 * although this case should be even more rare in practice.
	 */
	sockevent_raise(sock, SEV_RECV);
}

/*
 * Cancel a previous request that may currently be suspended.  The cancel
 * operation itself does not have a reply.  Instead, if the given request was
 * found to be suspended, that request must be aborted and an appropriate reply
 * must be sent for the request.  If no matching request was found, no reply
 * must be sent at all.
 */
static void
sockevent_cancel(sockid_t id, const struct sockdriver_call * call)
{
	struct sockevent_proc *spr;
	struct sock *sock;

	/*
	 * Due to asynchronous close(2) operations, not even the sock object
	 * may be found.  If this (entirely legitimate) case, do not send any
	 * reply.
	 */
	if ((sock = sockhash_get(id)) == NULL)
		return;

	/*
	 * The request may already have completed by the time we receive the
	 * cancel request, in which case we can not find it.  In this (entirely
	 * legitimate) case, do not send any reply.
	 */
	if ((spr = sockevent_unsuspend(sock, call)) == NULL)
		return;

	/*
	 * We found the operation.  Cancel it according to its call type.
	 * Then, once fully done with it, free the suspension data structure.
	 *
	 * Note that we have to use the call structure from the suspension data
	 * structure rather than the given 'call' pointer: only the former
	 * includes all the information necessary to resume the request!
	 */
	switch (spr->spr_event) {
	case SEV_BIND:
	case SEV_CONNECT:
		assert(spr->spr_call.sc_endpt != NONE);

		sockdriver_reply_generic(&spr->spr_call, EINTR);

		break;

	case SEV_ACCEPT:
		sockdriver_reply_accept(&spr->spr_call, EINTR, NULL, 0);

		break;

	case SEV_SEND:
		sockevent_cancel_send(sock, spr, EINTR);

		break;

	case SEV_RECV:
		sockevent_cancel_recv(sock, spr, EINTR);

		break;

	case SEV_CLOSE:
		/*
		 * Return EINPROGRESS rather than EINTR, so that the user
		 * process can tell from the close(2) result that the file
		 * descriptor has in fact been closed.
		 */
		sockdriver_reply_generic(&spr->spr_call, EINPROGRESS);

		/*
		 * Do not free the sock object here: the socket driver will
		 * complete the close in the background, and fire SEV_CLOSE
		 * once it is done.  Only then is the sock object freed.
		 */
		break;

	default:
		panic("libsockevent: process suspended on unknown event 0x%x",
		    spr->spr_event);
	}

	sockevent_proc_free(spr);
}

/*
 * Process a select request.
 */
static int
sockevent_select(sockid_t id, unsigned int ops,
	const struct sockdriver_select * sel)
{
	struct sock *sock;
	unsigned int r, notify;

	if ((sock = sockhash_get(id)) == NULL)
		return EINVAL;

	notify = (ops & SDEV_NOTIFY);
	ops &= (SDEV_OP_RD | SDEV_OP_WR | SDEV_OP_ERR);

	/*
	 * See if any of the requested select operations can be satisfied
	 * immediately.
	 */
	r = sockevent_test_select(sock, ops);

	/*
	 * If select operations were pending, the new results must not indicate
	 * that any of those were satisfied, as that would indicate an internal
	 * logic error: the socket driver is supposed to update its state
	 * proactively, and thus, discovering that things have changed here is
	 * not something that should ever happen.
	 */
	assert(!(sock->sock_selops & r));

	/*
	 * If any select operations are not satisfied immediately, and we are
	 * asked to notify the caller when they are satisfied later, save them
	 * for later retesting.
	 */
	ops &= ~r;

	if (notify && ops != 0) {
		/*
		 * For now, we support only one caller when it comes to select
		 * queries: VFS.  If we want to support a networked file system
		 * (or so) directly calling select as well, this library will
		 * have to be extended accordingly (should not be too hard).
		 */
		if (sock->sock_select.ss_endpt != NONE) {
			if (sock->sock_select.ss_endpt != sel->ss_endpt) {
				printf("libsockevent: no support for multiple "
				    "select callers yet\n");

				return EIO;
			}

			/*
			 * If a select query was already pending for this
			 * caller, we must simply merge in the new operations.
			 */
			sock->sock_selops |= ops;
		} else {
			assert(sel->ss_endpt != NONE);

			sock->sock_select = *sel;
			sock->sock_selops = ops;
		}
	}

	return r;
}

/*
 * An alarm has triggered.  Expire any timers.  Socket drivers that do not pass
 * clock notification messages to libsockevent must call expire_timers(3)
 * themselves instead.
 */
static void
sockevent_alarm(clock_t now)
{

	expire_timers(now);
}

static const struct sockdriver sockevent_tab = {
	.sdr_socket		= sockevent_socket,
	.sdr_socketpair		= sockevent_socketpair,
	.sdr_bind		= sockevent_bind,
	.sdr_connect		= sockevent_connect,
	.sdr_listen		= sockevent_listen,
	.sdr_accept		= sockevent_accept,
	.sdr_send		= sockevent_send,
	.sdr_recv		= sockevent_recv,
	.sdr_ioctl		= sockevent_ioctl,
	.sdr_setsockopt		= sockevent_setsockopt,
	.sdr_getsockopt		= sockevent_getsockopt,
	.sdr_getsockname	= sockevent_getsockname,
	.sdr_getpeername	= sockevent_getpeername,
	.sdr_shutdown		= sockevent_shutdown,
	.sdr_close		= sockevent_close,
	.sdr_cancel		= sockevent_cancel,
	.sdr_select		= sockevent_select,
	.sdr_alarm		= sockevent_alarm
};

/*
 * Initialize the socket event library.
 */
void
sockevent_init(sockevent_socket_cb_t socket_cb)
{

	sockhash_init();

	socktimer_init();

	sockevent_proc_init();

	SIMPLEQ_INIT(&sockevent_pending);

	assert(socket_cb != NULL);
	sockevent_socket_cb = socket_cb;

	/* Announce we are up. */
	sockdriver_announce();

	sockevent_working = FALSE;
}

/*
 * Process a socket driver request message.
 */
void
sockevent_process(const message * m_ptr, int ipc_status)
{

	/* Block events until after we have processed the request. */
	assert(!sockevent_working);
	sockevent_working = TRUE;

	/* Actually process the request. */
	sockdriver_process(&sockevent_tab, m_ptr, ipc_status);

	/*
	 * If any events were fired while processing the request, they will
	 * have been queued for later.  Go through them now.
	 */
	if (sockevent_has_events())
		sockevent_pump();

	sockevent_working = FALSE;
}
