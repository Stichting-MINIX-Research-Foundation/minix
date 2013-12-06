/*	$NetBSD: clnt_vc.c,v 1.24 2013/10/17 23:58:05 christos Exp $	*/

/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the "Oracle America, Inc." nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)clnt_tcp.c 1.37 87/10/05 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)clnt_tcp.c	2.2 88/08/01 4.0 RPCSRC";
static char sccsid[] = "@(#)clnt_vc.c 1.19 89/03/16 Copyr 1988 Sun Micro";
#else
__RCSID("$NetBSD: clnt_vc.c,v 1.24 2013/10/17 23:58:05 christos Exp $");
#endif
#endif
 
/*
 * clnt_tcp.c, Implements a TCP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * TCP based RPC supports 'batched calls'.
 * A sequence of calls may be batched-up in a send buffer.  The rpc call
 * return immediately to the client even though the call was not necessarily
 * sent.  The batching occurs if the results' xdr routine is NULL (0) AND
 * the rpc timeout value is zero (see clnt.h, rpc).
 *
 * Clients should NOT casually batch calls that in fact return results; that is,
 * the server side should be aware that a call is batched and not produce any
 * return message.  Batched calls that produce many result messages can
 * deadlock (netlock) the client and the server....
 *
 * Now go hang yourself.
 */

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <rpc/rpc.h>

#include "svc_fdset.h"
#include "rpc_internal.h"

#ifdef __weak_alias
__weak_alias(clnt_vc_create,_clnt_vc_create)
#endif

#define MCALL_MSG_SIZE 24

static enum clnt_stat clnt_vc_call(CLIENT *, rpcproc_t, xdrproc_t,
    const char *, xdrproc_t, caddr_t, struct timeval);
static void clnt_vc_geterr(CLIENT *, struct rpc_err *);
static bool_t clnt_vc_freeres(CLIENT *, xdrproc_t, caddr_t);
static void clnt_vc_abort(CLIENT *);
static bool_t clnt_vc_control(CLIENT *, u_int, char *);
static void clnt_vc_destroy(CLIENT *);
static struct clnt_ops *clnt_vc_ops(void);
static bool_t time_not_ok(struct timeval *);
static int read_vc(caddr_t, caddr_t, int);
static int write_vc(caddr_t, caddr_t, int);

struct ct_data {
	int		ct_fd;
	bool_t		ct_closeit;
	struct timeval	ct_wait;
	bool_t          ct_waitset;       /* wait set by clnt_control? */
	struct netbuf	ct_addr; 
	struct rpc_err	ct_error;
	union {
		char	ct_mcallc[MCALL_MSG_SIZE];	/* marshalled callmsg */
		u_int32_t ct_mcalli;
	} ct_u;
	u_int		ct_mpos;			/* pos after marshal */
	XDR		ct_xdrs;
};

/*
 *      This machinery implements per-fd locks for MT-safety.  It is not
 *      sufficient to do per-CLIENT handle locks for MT-safety because a
 *      user may create more than one CLIENT handle with the same fd behind
 *      it.  Therfore, we allocate an array of flags (vc_fd_locks), protected
 *      by the clnt_fd_lock mutex, and an array (vc_cv) of condition variables
 *      similarly protected.  Vc_fd_lock[fd] == 1 => a call is activte on some
 *      CLIENT handle created for that fd.
 *      The current implementation holds locks across the entire RPC and reply.
 *      Yes, this is silly, and as soon as this code is proven to work, this
 *      should be the first thing fixed.  One step at a time.
 */
#ifdef _REENTRANT
static int      *vc_fd_locks;
#define __rpc_lock_value __isthreaded;
extern mutex_t  clnt_fd_lock;
static cond_t   *vc_cv;
#define release_fd_lock(fd, mask) {             \
	mutex_lock(&clnt_fd_lock);      \
	vc_fd_locks[fd] = 0;            \
	mutex_unlock(&clnt_fd_lock);    \
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);        \
	cond_signal(&vc_cv[fd]);        \
}
#else
#define release_fd_lock(fd,mask)
#define __rpc_lock_value 0
#endif

static __inline void
htonlp(void *dst, const void *src)
{
#if 0
	uint32_t tmp;
	memcpy(&tmp, src, sizeof(tmp));
	tmp = htonl(tmp);
	memcpy(dst, &tmp, sizeof(tmp));
#else
	/* We are aligned, so we think */
	*(uint32_t *)dst = htonl(*(const uint32_t *)src);
#endif
}

static __inline void
ntohlp(void *dst, const void *src)
{
#if 0
	uint32_t tmp;
	memcpy(&tmp, src, sizeof(tmp));
	tmp = ntohl(tmp);
	memcpy(dst, &tmp, sizeof(tmp));
#else
	/* We are aligned, so we think */
	*(uint32_t *)dst = htonl(*(const uint32_t *)src);
#endif
}

/*
 * Create a client handle for a connection.
 * Default options are set, which the user can change using clnt_control()'s.
 * The rpc/vc package does buffering similar to stdio, so the client
 * must pick send and receive buffer sizes, 0 => use the default.
 * NB: fd is copied into a private area.
 * NB: The rpch->cl_auth is set null authentication. Caller may wish to
 * set this something more useful.
 *
 * fd should be an open socket
 */
CLIENT *
clnt_vc_create(
	int fd,
	const struct netbuf *raddr,
	rpcprog_t prog,
	rpcvers_t vers,
	u_int sendsz,
	u_int recvsz
)
{
	CLIENT *h;
	struct ct_data *ct = NULL;
	struct rpc_msg call_msg;
#ifdef _REENTRANT
	sigset_t mask;
#endif
	sigset_t newmask;
	struct sockaddr_storage ss;
	socklen_t slen;
	struct __rpc_sockinfo si;

	_DIAGASSERT(raddr != NULL);

	h  = mem_alloc(sizeof(*h));
	if (h == NULL) {
		warnx("clnt_vc_create: out of memory");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	ct = mem_alloc(sizeof(*ct));
	if (ct == NULL) {
		warnx("clnt_vc_create: out of memory");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}

	__clnt_sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
#ifdef _REENTRANT
	mutex_lock(&clnt_fd_lock);
	if (vc_fd_locks == NULL) {
		size_t cv_allocsz, fd_allocsz;
		int dtbsize = __rpc_dtbsize();

		fd_allocsz = dtbsize * sizeof (int);
		vc_fd_locks = mem_alloc(fd_allocsz);
		if (vc_fd_locks == NULL) {
			goto blooy;
		} else
			memset(vc_fd_locks, '\0', fd_allocsz);

		_DIAGASSERT(vc_cv == NULL);
		cv_allocsz = dtbsize * sizeof (cond_t);
		vc_cv = mem_alloc(cv_allocsz);
		if (vc_cv == NULL) {
			mem_free(vc_fd_locks, fd_allocsz);
			vc_fd_locks = NULL;
			goto blooy;
		} else {
			int i;

			for (i = 0; i < dtbsize; i++)
				cond_init(&vc_cv[i], 0, (void *) 0);
		}
	} else
		_DIAGASSERT(vc_cv != NULL);
#endif

	/*
	 * XXX - fvdl connecting while holding a mutex?
	 */
	slen = sizeof ss;
	if (getpeername(fd, (struct sockaddr *)(void *)&ss, &slen) < 0) {
		if (errno != ENOTCONN) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			goto blooy;
		}
		if (connect(fd, (struct sockaddr *)raddr->buf, raddr->len) < 0){
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			goto blooy;
		}
	}
	mutex_unlock(&clnt_fd_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
	if (!__rpc_fd2sockinfo(fd, &si))
		goto fooy;

	ct->ct_closeit = FALSE;

	/*
	 * Set up private data struct
	 */
	ct->ct_fd = fd;
	ct->ct_wait.tv_usec = 0;
	ct->ct_waitset = FALSE;
	ct->ct_addr.buf = malloc((size_t)raddr->maxlen);
	if (ct->ct_addr.buf == NULL)
		goto fooy;
	memcpy(ct->ct_addr.buf, raddr->buf, (size_t)raddr->len);
	ct->ct_addr.len = raddr->len;
	ct->ct_addr.maxlen = raddr->maxlen;

	/*
	 * Initialize call message
	 */
	call_msg.rm_xid = __RPC_GETXID();
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = (u_int32_t)prog;
	call_msg.rm_call.cb_vers = (u_int32_t)vers;

	/*
	 * pre-serialize the static part of the call msg and stash it away
	 */
	xdrmem_create(&(ct->ct_xdrs), ct->ct_u.ct_mcallc, MCALL_MSG_SIZE,
	    XDR_ENCODE);
	if (! xdr_callhdr(&(ct->ct_xdrs), &call_msg)) {
		if (ct->ct_closeit) {
			(void)close(fd);
		}
		goto fooy;
	}
	ct->ct_mpos = XDR_GETPOS(&(ct->ct_xdrs));
	XDR_DESTROY(&(ct->ct_xdrs));

	/*
	 * Create a client handle which uses xdrrec for serialization
	 * and authnone for authentication.
	 */
	h->cl_ops = clnt_vc_ops();
	h->cl_private = ct;
	h->cl_auth = authnone_create();
	sendsz = __rpc_get_t_size(si.si_af, si.si_proto, (int)sendsz);
	recvsz = __rpc_get_t_size(si.si_af, si.si_proto, (int)recvsz);
	xdrrec_create(&(ct->ct_xdrs), sendsz, recvsz,
	    h->cl_private, read_vc, write_vc);
	return (h);

blooy:
	mutex_unlock(&clnt_fd_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
fooy:
	/*
	 * Something goofed, free stuff and barf
	 */
	if (ct)
		mem_free(ct, sizeof(struct ct_data));
	if (h)
		mem_free(h, sizeof(CLIENT));
	return (NULL);
}

static enum clnt_stat
clnt_vc_call(
	CLIENT *h,
	rpcproc_t proc,
	xdrproc_t xdr_args,
	const char *args_ptr,
	xdrproc_t xdr_results,
	caddr_t results_ptr,
	struct timeval timeout
)
{
	struct ct_data *ct;
	XDR *xdrs;
	struct rpc_msg reply_msg;
	u_int32_t x_id;
	u_int32_t *msg_x_id;
	bool_t shipnow;
	int refreshes = 2;
#ifdef _REENTRANT
	sigset_t mask, newmask;
#endif

	_DIAGASSERT(h != NULL);

	ct = (struct ct_data *) h->cl_private;

#ifdef _REENTRANT
	__clnt_sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
	while (vc_fd_locks[ct->ct_fd])
		cond_wait(&vc_cv[ct->ct_fd], &clnt_fd_lock);
	vc_fd_locks[ct->ct_fd] = __rpc_lock_value;
	mutex_unlock(&clnt_fd_lock);
#endif

	xdrs = &(ct->ct_xdrs);
	msg_x_id = &ct->ct_u.ct_mcalli;

	if (!ct->ct_waitset) {
		if (time_not_ok(&timeout) == FALSE)
		ct->ct_wait = timeout;
	}

	shipnow =
	    (xdr_results == NULL && timeout.tv_sec == 0
	    && timeout.tv_usec == 0) ? FALSE : TRUE;

call_again:
	xdrs->x_op = XDR_ENCODE;
	ct->ct_error.re_status = RPC_SUCCESS;
	x_id = ntohl(--(*msg_x_id));
	if ((! XDR_PUTBYTES(xdrs, ct->ct_u.ct_mcallc, ct->ct_mpos)) ||
	    (! XDR_PUTINT32(xdrs, (int32_t *)&proc)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*xdr_args)(xdrs, __UNCONST(args_ptr)))) {
		if (ct->ct_error.re_status == RPC_SUCCESS)
			ct->ct_error.re_status = RPC_CANTENCODEARGS;
		(void)xdrrec_endofrecord(xdrs, TRUE);
		release_fd_lock(ct->ct_fd, mask);
		return (ct->ct_error.re_status);
	}
	if (! xdrrec_endofrecord(xdrs, shipnow)) {
		release_fd_lock(ct->ct_fd, mask);
		return (ct->ct_error.re_status = RPC_CANTSEND);
	}
	if (! shipnow) {
		release_fd_lock(ct->ct_fd, mask);
		return (RPC_SUCCESS);
	}
	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		release_fd_lock(ct->ct_fd, mask);
		return(ct->ct_error.re_status = RPC_TIMEDOUT);
	}


	/*
	 * Keep receiving until we get a valid transaction id
	 */
	xdrs->x_op = XDR_DECODE;
	for (;;) {
		reply_msg.acpted_rply.ar_verf = _null_auth;
		reply_msg.acpted_rply.ar_results.where = NULL;
		reply_msg.acpted_rply.ar_results.proc = (xdrproc_t)xdr_void;
		if (! xdrrec_skiprecord(xdrs)) {
			release_fd_lock(ct->ct_fd, mask);
			return (ct->ct_error.re_status);
		}
		/* now decode and validate the response header */
		if (! xdr_replymsg(xdrs, &reply_msg)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				continue;
			release_fd_lock(ct->ct_fd, mask);
			return (ct->ct_error.re_status);
		}
		if (reply_msg.rm_xid == x_id)
			break;
	}

	/*
	 * process header
	 */
	_seterr_reply(&reply_msg, &(ct->ct_error));
	if (ct->ct_error.re_status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth,
		    &reply_msg.acpted_rply.ar_verf)) {
			ct->ct_error.re_status = RPC_AUTHERROR;
			ct->ct_error.re_why = AUTH_INVALIDRESP;
		} else if (! (*xdr_results)(xdrs, results_ptr)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				ct->ct_error.re_status = RPC_CANTDECODERES;
		}
		/* free verifier ... */
		if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs,
			    &(reply_msg.acpted_rply.ar_verf));
		}
	}  /* end successful completion */
	else {
		/* maybe our credentials need to be refreshed ... */
		if (refreshes-- && AUTH_REFRESH(h->cl_auth))
			goto call_again;
	}  /* end of unsuccessful completion */
	release_fd_lock(ct->ct_fd, mask);
	return (ct->ct_error.re_status);
}

static void
clnt_vc_geterr(
	CLIENT *h,
	struct rpc_err *errp
)
{
	struct ct_data *ct;

	_DIAGASSERT(h != NULL);
	_DIAGASSERT(errp != NULL);

	ct = (struct ct_data *) h->cl_private;
	*errp = ct->ct_error;
}

static bool_t
clnt_vc_freeres(
	CLIENT *cl,
	xdrproc_t xdr_res,
	caddr_t res_ptr
)
{
	struct ct_data *ct;
	XDR *xdrs;
	bool_t dummy;
#ifdef _REENTRANT
	sigset_t mask;
#endif
	sigset_t newmask;

	_DIAGASSERT(cl != NULL);

	ct = (struct ct_data *)cl->cl_private;
	xdrs = &(ct->ct_xdrs);

	__clnt_sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
#ifdef _REENTRANT
	while (vc_fd_locks[ct->ct_fd])
		cond_wait(&vc_cv[ct->ct_fd], &clnt_fd_lock);
#endif

	xdrs->x_op = XDR_FREE;
	dummy = (*xdr_res)(xdrs, res_ptr);
	mutex_unlock(&clnt_fd_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
	cond_signal(&vc_cv[ct->ct_fd]);

	return dummy;
}

/*ARGSUSED*/
static void
clnt_vc_abort(CLIENT *cl)
{
}

static bool_t
clnt_vc_control(
	CLIENT *cl,
	u_int request,
	char *info
)
{
	struct ct_data *ct;
	void *infop = info;
#ifdef _REENTRANT
	sigset_t mask;
#endif
	sigset_t newmask;

	_DIAGASSERT(cl != NULL);

	ct = (struct ct_data *)cl->cl_private;

	__clnt_sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
#ifdef _REENTRANT
	while (vc_fd_locks[ct->ct_fd])
		cond_wait(&vc_cv[ct->ct_fd], &clnt_fd_lock);
	vc_fd_locks[ct->ct_fd] = __rpc_lock_value;
#endif
	mutex_unlock(&clnt_fd_lock);

	switch (request) {
	case CLSET_FD_CLOSE:
		ct->ct_closeit = TRUE;
		release_fd_lock(ct->ct_fd, mask);
		return (TRUE);
	case CLSET_FD_NCLOSE:
		ct->ct_closeit = FALSE;
		release_fd_lock(ct->ct_fd, mask);
		return (TRUE);
	default:
		break;
	}

	/* for other requests which use info */
	if (info == NULL) {
		release_fd_lock(ct->ct_fd, mask);
		return (FALSE);
	}
	switch (request) {
	case CLSET_TIMEOUT:
		if (time_not_ok((struct timeval *)(void *)info)) {
			release_fd_lock(ct->ct_fd, mask);
			return (FALSE);
		}
		ct->ct_wait = *(struct timeval *)infop;
		ct->ct_waitset = TRUE;
		break;
	case CLGET_TIMEOUT:
		*(struct timeval *)infop = ct->ct_wait;
		break;
	case CLGET_SERVER_ADDR:
		(void) memcpy(info, ct->ct_addr.buf, (size_t)ct->ct_addr.len);
		break;
	case CLGET_FD:
		*(int *)(void *)info = ct->ct_fd;
		break;
	case CLGET_SVC_ADDR:
		/* The caller should not free this memory area */
		*(struct netbuf *)(void *)info = ct->ct_addr;
		break;
	case CLSET_SVC_ADDR:		/* set to new address */
		release_fd_lock(ct->ct_fd, mask);
		return (FALSE);
	case CLGET_XID:
		/*
		 * use the knowledge that xid is the
		 * first element in the call structure
		 * This will get the xid of the PREVIOUS call
		 */
		ntohlp(info, &ct->ct_u.ct_mcalli);
		break;
	case CLSET_XID:
		/* This will set the xid of the NEXT call */
		htonlp(&ct->ct_u.ct_mcalli, (const char *)info +
		    sizeof(uint32_t));
		/* increment by 1 as clnt_vc_call() decrements once */
		break;
	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		ntohlp(info, ct->ct_u.ct_mcallc + 4 * BYTES_PER_XDR_UNIT);
		break;

	case CLSET_VERS:
		htonlp(ct->ct_u.ct_mcallc + 4 * BYTES_PER_XDR_UNIT, info);
		break;

	case CLGET_PROG:
		/*
		 * This RELIES on the information that, in the call body,
		 * the program number field is the fourth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		ntohlp(info, ct->ct_u.ct_mcallc + 3 * BYTES_PER_XDR_UNIT);
		break;

	case CLSET_PROG:
		htonlp(ct->ct_u.ct_mcallc + 3 * BYTES_PER_XDR_UNIT, info);
		break;

	default:
		release_fd_lock(ct->ct_fd, mask);
		return (FALSE);
	}
	release_fd_lock(ct->ct_fd, mask);
	return (TRUE);
}


static void
clnt_vc_destroy(CLIENT *cl)
{
	struct ct_data *ct;
#ifdef _REENTRANT
	int ct_fd;
	sigset_t mask;
#endif
	sigset_t newmask;

	_DIAGASSERT(cl != NULL);

	ct = (struct ct_data *) cl->cl_private;
	ct_fd = ct->ct_fd;

	__clnt_sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
#ifdef _REENTRANT
	while (vc_fd_locks[ct_fd])
		cond_wait(&vc_cv[ct_fd], &clnt_fd_lock);
#endif
	if (ct->ct_closeit && ct->ct_fd != -1) {
		(void)close(ct->ct_fd);
	}
	XDR_DESTROY(&(ct->ct_xdrs));
	if (ct->ct_addr.buf)
		free(ct->ct_addr.buf);
	mem_free(ct, sizeof(struct ct_data));
	mem_free(cl, sizeof(CLIENT));
	mutex_unlock(&clnt_fd_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);

	cond_signal(&vc_cv[ct_fd]);
}

/*
 * Interface between xdr serializer and tcp connection.
 * Behaves like the system calls, read & write, but keeps some error state
 * around for the rpc level.
 */
static int
read_vc(char *ctp, char *buf, int len)
{
	struct ct_data *ct = (struct ct_data *)(void *)ctp;
	struct pollfd fd;
	struct timespec ts;
	ssize_t nread;

	if (len == 0)
		return (0);

	TIMEVAL_TO_TIMESPEC(&ct->ct_wait, &ts);
	fd.fd = ct->ct_fd;
	fd.events = POLLIN;
	for (;;) {
		switch (pollts(&fd, 1, &ts, NULL)) {
		case 0:
			ct->ct_error.re_status = RPC_TIMEDOUT;
			return (-1);

		case -1:
			if (errno == EINTR)
				continue;
			ct->ct_error.re_status = RPC_CANTRECV;
			ct->ct_error.re_errno = errno;
			return (-1);
		}
		break;
	}
	switch (nread = read(ct->ct_fd, buf, (size_t)len)) {

	case 0:
		/* premature eof */
		ct->ct_error.re_errno = ECONNRESET;
		ct->ct_error.re_status = RPC_CANTRECV;
		nread = -1;  /* it's really an error */
		break;

	case -1:
		ct->ct_error.re_errno = errno;
		ct->ct_error.re_status = RPC_CANTRECV;
		break;
	}
	return (int)nread;
}

static int
write_vc(char *ctp, char *buf, int len)
{
	struct ct_data *ct = (struct ct_data *)(void *)ctp;
	ssize_t i;
	size_t cnt;

	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		if ((i = write(ct->ct_fd, buf, cnt)) == -1) {
			ct->ct_error.re_errno = errno;
			ct->ct_error.re_status = RPC_CANTSEND;
			return (-1);
		}
	}
	return len;
}

static struct clnt_ops *
clnt_vc_ops(void)
{
	static struct clnt_ops ops;
#ifdef _REENTRANT
	extern mutex_t  ops_lock;
	sigset_t mask;
#endif
	sigset_t newmask;

	/* VARIABLES PROTECTED BY ops_lock: ops */

	__clnt_sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&ops_lock);
	if (ops.cl_call == NULL) {
		ops.cl_call = clnt_vc_call;
		ops.cl_abort = clnt_vc_abort;
		ops.cl_geterr = clnt_vc_geterr;
		ops.cl_freeres = clnt_vc_freeres;
		ops.cl_destroy = clnt_vc_destroy;
		ops.cl_control = clnt_vc_control;
	}
	mutex_unlock(&ops_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
	return (&ops);
}

/*
 * Make sure that the time is not garbage.   -1 value is disallowed.
 * Note this is different from time_not_ok in clnt_dg.c
 */
static bool_t
time_not_ok(struct timeval *t)
{

	_DIAGASSERT(t != NULL);

	return (t->tv_sec <= -1 || t->tv_sec > 100000000 ||
		t->tv_usec <= -1 || t->tv_usec > 1000000);
}
