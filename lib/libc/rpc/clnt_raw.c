/*	$NetBSD: clnt_raw.c,v 1.32 2013/03/11 20:19:29 tron Exp $	*/

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
static char *sccsid = "@(#)clnt_raw.c 1.22 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)clnt_raw.c	2.2 88/08/01 4.0 RPCSRC";
#else
__RCSID("$NetBSD: clnt_raw.c,v 1.32 2013/03/11 20:19:29 tron Exp $");
#endif
#endif

/*
 * clnt_raw.c
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * Memory based rpc for simple testing and timing.
 * Interface to create an rpc client and server in the same process.
 * This lets us similate rpc and get round trip overhead, without
 * any interference from the kernel.
 */

#include "namespace.h"
#include "reentrant.h"
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include <rpc/rpc.h>
#include <rpc/raw.h>

#ifdef __weak_alias
__weak_alias(clntraw_create,_clntraw_create)
__weak_alias(clnt_raw_create,_clnt_raw_create)
#endif

#ifdef _REENTRANT
extern mutex_t clntraw_lock;
#endif

#define MCALL_MSG_SIZE 24

/*
 * This is the "network" we will be moving stuff over.
 */
static struct clntraw_private {
	CLIENT	client_object;
	XDR	xdr_stream;
	char	*_raw_buf;
	union {
	    struct rpc_msg	mashl_rpcmsg;
	    char 		mashl_callmsg[MCALL_MSG_SIZE];
	} u;
	u_int	mcnt;
} *clntraw_private;

static enum clnt_stat clnt_raw_call(CLIENT *, rpcproc_t, xdrproc_t,
    const char *, xdrproc_t, caddr_t, struct timeval);
static void clnt_raw_geterr(CLIENT *, struct rpc_err *);
static bool_t clnt_raw_freeres(CLIENT *, xdrproc_t, caddr_t);
static void clnt_raw_abort(CLIENT *);
static bool_t clnt_raw_control(CLIENT *, u_int, char *);
static void clnt_raw_destroy(CLIENT *);
static struct clnt_ops *clnt_raw_ops(void);

/*
 * Create a client handle for memory based rpc.
 */
CLIENT *
clnt_raw_create(rpcprog_t prog, rpcvers_t vers)
{
	struct clntraw_private *clp = clntraw_private;
	struct rpc_msg call_msg;
	XDR *xdrs = &clp->xdr_stream;
	CLIENT	*client = &clp->client_object;

	mutex_lock(&clntraw_lock);
	if (clp == NULL) {
		clp = calloc((size_t)1, sizeof (*clp));
		if (clp == NULL)
			goto out;
		if (__rpc_rawcombuf == NULL)
			__rpc_rawcombuf =
			    malloc(UDPMSGSIZE);
		if (__rpc_rawcombuf == NULL)
			goto out;
		clp->_raw_buf = __rpc_rawcombuf;
		clntraw_private = clp;
	}
	/*
	 * pre-serialize the static part of the call msg and stash it away
	 */
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	/* XXX: prog and vers have been long historically :-( */
	call_msg.rm_call.cb_prog = (u_int32_t)prog;
	call_msg.rm_call.cb_vers = (u_int32_t)vers;
	xdrmem_create(xdrs, clp->u.mashl_callmsg, MCALL_MSG_SIZE, XDR_ENCODE); 
	if (! xdr_callhdr(xdrs, &call_msg))
		warnx("%s: Fatal header serialization error", __func__);
	clp->mcnt = XDR_GETPOS(xdrs);
	XDR_DESTROY(xdrs);

	/*
	 * Set xdrmem for client/server shared buffer
	 */
	xdrmem_create(xdrs, clp->_raw_buf, UDPMSGSIZE, XDR_FREE);

	/*
	 * create client handle
	 */
	client->cl_ops = clnt_raw_ops();
	client->cl_auth = authnone_create();
	mutex_unlock(&clntraw_lock);
	return (client);
out:
	if (clp)
		free(clp);
	mutex_unlock(&clntraw_lock);
	return NULL;

}

/* ARGSUSED */
static enum clnt_stat 
clnt_raw_call(CLIENT *h, rpcproc_t proc, xdrproc_t xargs, const char *argsp,
	xdrproc_t xresults, caddr_t resultsp, struct timeval timeout)
{
	struct clntraw_private *clp = clntraw_private;
	XDR *xdrs = &clp->xdr_stream;
	struct rpc_msg msg;
	enum clnt_stat status;
	struct rpc_err error;

	_DIAGASSERT(h != NULL);

	mutex_lock(&clntraw_lock);
	if (clp == NULL) {
		mutex_unlock(&clntraw_lock);
		return (RPC_FAILED);
	}
	mutex_unlock(&clntraw_lock);

call_again:
	/*
	 * send request
	 */
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	clp->u.mashl_rpcmsg.rm_xid ++ ;
	if ((! XDR_PUTBYTES(xdrs, clp->u.mashl_callmsg, clp->mcnt)) ||
	    (! XDR_PUTINT32(xdrs, (int32_t *)&proc)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*xargs)(xdrs, __UNCONST(argsp)))) {
		return (RPC_CANTENCODEARGS);
	}
	(void)XDR_GETPOS(xdrs);  /* called just to cause overhead */

	/*
	 * We have to call server input routine here because this is
	 * all going on in one process. Yuk.
	 */
	svc_getreq_common(FD_SETSIZE);

	/*
	 * get results
	 */
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	msg.acpted_rply.ar_verf = _null_auth;
	msg.acpted_rply.ar_results.where = resultsp;
	msg.acpted_rply.ar_results.proc = xresults;
	if (! xdr_replymsg(xdrs, &msg)) {
		/*
		 * It's possible for xdr_replymsg() to fail partway
		 * through its attempt to decode the result from the
		 * server. If this happens, it will leave the reply
		 * structure partially populated with dynamically
		 * allocated memory. (This can happen if someone uses
		 * clntudp_bufcreate() to create a CLIENT handle and
		 * specifies a receive buffer size that is too small.)
		 * This memory must be free()ed to avoid a leak.
		 */
		int op = xdrs->x_op;
		xdrs->x_op = XDR_FREE;
		xdr_replymsg(xdrs, &msg);
		xdrs->x_op = op;
		return (RPC_CANTDECODERES);
	}
	_seterr_reply(&msg, &error);
	status = error.re_status;

	if (status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, &msg.acpted_rply.ar_verf)) {
			status = RPC_AUTHERROR;
		}
	}  /* end successful completion */
	else {
		if (AUTH_REFRESH(h->cl_auth))
			goto call_again;
	}  /* end of unsuccessful completion */

	if (status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, &msg.acpted_rply.ar_verf)) {
			status = RPC_AUTHERROR;
		}
		if (msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs, &(msg.acpted_rply.ar_verf));
		}
	}

	return (status);
}

/*ARGSUSED*/
static void
clnt_raw_geterr(CLIENT *cl, struct rpc_err *error)
{
}


/* ARGSUSED */
static bool_t
clnt_raw_freeres(CLIENT *cl, xdrproc_t xdr_res, caddr_t res_ptr)
{
	struct clntraw_private *clp = clntraw_private;
	XDR *xdrs = &clp->xdr_stream;
	bool_t rval;

	mutex_lock(&clntraw_lock);
	if (clp == NULL) {
		rval = (bool_t) RPC_FAILED;
		mutex_unlock(&clntraw_lock);
		return (rval);
	}
	mutex_unlock(&clntraw_lock);
	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

/*ARGSUSED*/
static void
clnt_raw_abort(CLIENT *cl)
{
}

/*ARGSUSED*/
static bool_t
clnt_raw_control(CLIENT *cl, u_int ui, char *str)
{
	return (FALSE);
}

/*ARGSUSED*/
static void
clnt_raw_destroy(CLIENT *cl)
{
}

static struct clnt_ops *
clnt_raw_ops(void)
{
	static struct clnt_ops ops;
#ifdef _REENTRANT
	extern mutex_t  ops_lock;
#endif

	/* VARIABLES PROTECTED BY ops_lock: ops */

	mutex_lock(&ops_lock);
	if (ops.cl_call == NULL) {
		ops.cl_call = clnt_raw_call;
		ops.cl_abort = clnt_raw_abort;
		ops.cl_geterr = clnt_raw_geterr;
		ops.cl_freeres = clnt_raw_freeres;
		ops.cl_destroy = clnt_raw_destroy;
		ops.cl_control = clnt_raw_control;
	}
	mutex_unlock(&ops_lock);
	return (&ops);
}
