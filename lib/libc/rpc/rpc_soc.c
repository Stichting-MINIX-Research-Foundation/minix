/*	$NetBSD: rpc_soc.c,v 1.19 2014/05/28 14:45:57 christos Exp $	*/

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

/* #ident	"@(#)rpc_soc.c	1.17	94/04/24 SMI" */

/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)rpc_soc.c 1.41 89/05/02 Copyr 1988 Sun Micro";
#else
__RCSID("$NetBSD: rpc_soc.c,v 1.19 2014/05/28 14:45:57 christos Exp $");
#endif
#endif

#ifdef PORTMAP
/*
 * rpc_soc.c
 *
 * The backward compatibility routines for the earlier implementation
 * of RPC, where the only transports supported were tcp/ip and udp/ip.
 * Based on berkeley socket abstraction, now implemented on the top
 * of TLI/Streams
 */

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpc/nettype.h>
#include <netinet/in.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "svc_fdset.h"
#include "rpc_internal.h"

#ifdef __weak_alias
__weak_alias(clntudp_bufcreate,_clntudp_bufcreate)
__weak_alias(clntudp_create,_clntudp_create)
__weak_alias(clnttcp_create,_clnttcp_create)
__weak_alias(clntraw_create,_clntraw_create)
__weak_alias(get_myaddress,_get_myaddress)
__weak_alias(svcfd_create,_svcfd_create)
__weak_alias(svcudp_bufcreate,_svcudp_bufcreate)
__weak_alias(svcudp_create,_svcudp_create)
__weak_alias(svctcp_create,_svctcp_create)
__weak_alias(svcraw_create,_svcraw_create)
__weak_alias(callrpc,_callrpc)
__weak_alias(registerrpc,_registerrpc)
__weak_alias(clnt_broadcast,_clnt_broadcast)
#endif

#ifdef _REENTRANT
extern mutex_t	rpcsoc_lock;
#endif

static CLIENT *clnt_com_create(struct sockaddr_in *, rpcprog_t, rpcvers_t,
				    int *, u_int, u_int, const char *);
static SVCXPRT *svc_com_create(int, u_int, u_int, const char *);
static bool_t rpc_wrap_bcast(char *, struct netbuf *, struct netconfig *);

/*
 * A common clnt create routine
 */
static CLIENT *
clnt_com_create(struct sockaddr_in *raddr, rpcprog_t prog, rpcvers_t vers,
	int *sockp, u_int sendsz, u_int recvsz, const char *tp)
{
	CLIENT *cl;
	int madefd = FALSE;
	int fd;
	struct netconfig *nconf;
	struct netbuf bindaddr;

	_DIAGASSERT(raddr != NULL);
	_DIAGASSERT(sockp != NULL);
	_DIAGASSERT(tp != NULL);

	fd = *sockp;

	mutex_lock(&rpcsoc_lock);
	if ((nconf = __rpc_getconfip(tp)) == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		mutex_unlock(&rpcsoc_lock);
		return (NULL);
	}
	if (fd == RPC_ANYSOCK) {
		fd = __rpc_nconf2fd(nconf);
		if (fd == -1)
			goto syserror;
		madefd = TRUE;
	}

	if (raddr->sin_port == 0) {
		u_int proto;
		u_short sport;

		mutex_unlock(&rpcsoc_lock);	/* pmap_getport is recursive */
		proto = strcmp(tp, "udp") == 0 ? IPPROTO_UDP : IPPROTO_TCP;
		sport = pmap_getport(raddr, (u_long)prog, (u_long)vers,
		    proto);
		if (sport == 0) {
			goto err;
		}
		raddr->sin_port = htons(sport);
		mutex_lock(&rpcsoc_lock);	/* pmap_getport is recursive */
	}

	/* Transform sockaddr_in to netbuf */
	bindaddr.maxlen = bindaddr.len =  sizeof (struct sockaddr_in);
	bindaddr.buf = raddr;

	(void)bindresvport(fd, NULL);
	cl = clnt_tli_create(fd, nconf, &bindaddr, prog, vers,
				sendsz, recvsz);
	if (cl) {
		if (madefd == TRUE) {
			/*
			 * The fd should be closed while destroying the handle.
			 */
			(void) CLNT_CONTROL(cl, CLSET_FD_CLOSE, NULL);
			*sockp = fd;
		}
		(void) freenetconfigent(nconf);
		mutex_unlock(&rpcsoc_lock);
		return (cl);
	}
	goto err;

syserror:
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;

err:	if (madefd == TRUE)
		(void) close(fd);
	(void) freenetconfigent(nconf);
	mutex_unlock(&rpcsoc_lock);
	return (NULL);
}

CLIENT *
clntudp_bufcreate(struct sockaddr_in *raddr, u_long prog, u_long vers, struct timeval wait, int *sockp, u_int sendsz, u_int recvsz)
{
	CLIENT *cl;

	_DIAGASSERT(raddr != NULL);
	_DIAGASSERT(sockp != NULL);

	cl = clnt_com_create(raddr, (rpcprog_t)prog, (rpcvers_t)vers, sockp,
	    sendsz, recvsz, "udp");
	if (cl == NULL) {
		return (NULL);
	}
	(void) CLNT_CONTROL(cl, CLSET_RETRY_TIMEOUT, (char *)(void *)&wait);
	return (cl);
}

CLIENT *
clntudp_create(struct sockaddr_in *raddr, u_long program, u_long version,
    struct timeval wait, int *sockp)
{
	return clntudp_bufcreate(raddr, program, version, wait, sockp,
					UDPMSGSIZE, UDPMSGSIZE);
}

CLIENT *
clnttcp_create(struct sockaddr_in *raddr, u_long prog, u_long vers, int *sockp,
    u_int sendsz, u_int recvsz)
{
	return clnt_com_create(raddr, (rpcprog_t)prog, (rpcvers_t)vers, sockp,
	    sendsz, recvsz, "tcp");
}

CLIENT *
clntraw_create(u_long prog, u_long vers)
{
	return clnt_raw_create((rpcprog_t)prog, (rpcvers_t)vers);
}

/*
 * A common server create routine
 */
static SVCXPRT *
svc_com_create(int fd, u_int sendsize, u_int recvsize, const char *netid)
{
	struct netconfig *nconf;
	SVCXPRT *svc;
	int madefd = FALSE;
	int port;
	struct sockaddr_in sccsin;

	_DIAGASSERT(netid != NULL);

	if ((nconf = __rpc_getconfip(netid)) == NULL) {
		(void) syslog(LOG_ERR, "Could not get %s transport", netid);
		return (NULL);
	}
	if (fd == RPC_ANYSOCK) {
		fd = __rpc_nconf2fd(nconf);
		if (fd == -1) {
			(void) freenetconfigent(nconf);
			(void) syslog(LOG_ERR,
			"svc%s_create: could not open connection", netid);
			return (NULL);
		}
		madefd = TRUE;
	}

	memset(&sccsin, 0, sizeof sccsin);
	sccsin.sin_family = AF_INET;
	(void)bindresvport(fd, &sccsin);
	listen(fd, SOMAXCONN);
	svc = svc_tli_create(fd, nconf, NULL, sendsize, recvsize);
	(void) freenetconfigent(nconf);
	if (svc == NULL) {
		if (madefd)
			(void) close(fd);
		return (NULL);
	}
	port = (((struct sockaddr_in *)svc->xp_ltaddr.buf)->sin_port);
	svc->xp_port = ntohs(port);
	return (svc);
}

SVCXPRT *
svctcp_create(int fd, u_int sendsize, u_int recvsize)
{
	return svc_com_create(fd, sendsize, recvsize, "tcp");
}

SVCXPRT *
svcudp_bufcreate(int fd, u_int sendsz, u_int recvsz)
{
	return svc_com_create(fd, sendsz, recvsz, "udp");
}

SVCXPRT *
svcfd_create(int fd, u_int sendsize, u_int recvsize)
{
	return svc_fd_create(fd, sendsize, recvsize);
}


SVCXPRT *
svcudp_create(int fd)
{
	return svc_com_create(fd, UDPMSGSIZE, UDPMSGSIZE, "udp");
}

SVCXPRT *
svcraw_create(void)
{
	return svc_raw_create();
}

int
get_myaddress(struct sockaddr_in *addr)
{

	_DIAGASSERT(addr != NULL);

	memset((void *) addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(PMAPPORT);
	addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	return (0);
}

/*
 * For connectionless "udp" transport. Obsoleted by rpc_call().
 */
int
callrpc(char *host, int prognum, int versnum, int procnum,
	xdrproc_t inproc, char *in, xdrproc_t outproc, char *out)
{
	return (int)rpc_call(host, (rpcprog_t)prognum, (rpcvers_t)versnum,
	    (rpcproc_t)procnum, inproc, in, outproc, out, "udp");
}

/*
 * For connectionless kind of transport. Obsoleted by rpc_reg()
 */
int
registerrpc(int prognum, int versnum, int procnum,
	char *(*progname)(char [UDPMSGSIZE]),
	xdrproc_t inproc, xdrproc_t outproc)
{
	return rpc_reg((rpcprog_t)prognum, (rpcvers_t)versnum,
	    (rpcproc_t)procnum, progname, inproc, outproc, __UNCONST("udp"));
}

/*
 * All the following clnt_broadcast stuff is convulated; it supports
 * the earlier calling style of the callback function
 */
#ifdef _REENTRANT
static thread_key_t	clnt_broadcast_key;
#endif
static resultproc_t	clnt_broadcast_result_main;

/*
 * Need to translate the netbuf address into sockaddr_in address.
 * Dont care about netid here.
 */
/* ARGSUSED */
static bool_t
rpc_wrap_bcast(
	char *resultp,		/* results of the call */
	struct netbuf *addr,	/* address of the guy who responded */
	struct netconfig *nconf) /* Netconf of the transport */
{
	resultproc_t clnt_broadcast_result;

	_DIAGASSERT(resultp != NULL);
	_DIAGASSERT(addr != NULL);
	_DIAGASSERT(nconf != NULL);

	if (strcmp(nconf->nc_netid, "udp"))
		return (FALSE);
#ifdef _REENTRANT
	if (__isthreaded == 0)
		clnt_broadcast_result = clnt_broadcast_result_main;
	else
		clnt_broadcast_result = thr_getspecific(clnt_broadcast_key);
#else
	clnt_broadcast_result = clnt_broadcast_result_main;
#endif
	return (*clnt_broadcast_result)(resultp,
				(struct sockaddr_in *)addr->buf);
}

#ifdef _REENTRANT
static once_t clnt_broadcast_once = ONCE_INITIALIZER;

static void
clnt_broadcast_setup(void)
{

	thr_keycreate(&clnt_broadcast_key, free);
}
#endif

/*
 * Broadcasts on UDP transport. Obsoleted by rpc_broadcast().
 */
enum clnt_stat
clnt_broadcast(
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	caddr_t		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	caddr_t		resultsp,	/* pointer to results */
	resultproc_t	eachresult)	/* call with each result obtained */
{
#ifdef _REENTRANT
	if (__isthreaded == 0)
		clnt_broadcast_result_main = eachresult;
	else {
		thr_once(&clnt_broadcast_once, clnt_broadcast_setup);
		thr_setspecific(clnt_broadcast_key, (void *) eachresult);
	}
#else
	clnt_broadcast_result_main = eachresult;
#endif
	return rpc_broadcast((rpcprog_t)prog, (rpcvers_t)vers,
	    (rpcproc_t)proc, xargs, argsp, xresults, resultsp,
	    (resultproc_t) rpc_wrap_bcast, "udp");
}

#endif /* PORTMAP */
