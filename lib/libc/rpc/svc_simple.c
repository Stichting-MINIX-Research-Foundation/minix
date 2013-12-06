/*	$NetBSD: svc_simple.c,v 1.33 2013/03/11 20:19:29 tron Exp $	*/

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
/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

/* #pragma ident	"@(#)svc_simple.c	1.18	94/04/24 SMI" */

/*
 * svc_simple.c
 * Simplified front end to rpc.
 */

/*
 * This interface creates a virtual listener for all the services
 * started thru rpc_reg(). It listens on the same endpoint for
 * all the services and then executes the corresponding service
 * for the given prognum and procnum.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: svc_simple.c,v 1.33 2013/03/11 20:19:29 tron Exp $");
#endif

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpc/nettype.h>
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpc_internal.h"

#ifdef __weak_alias
__weak_alias(rpc_reg,_rpc_reg)
#endif

static void universal(struct svc_req *, SVCXPRT *);

static struct proglst {
	char *(*p_progname)(char *);
	rpcprog_t p_prognum;
	rpcvers_t p_versnum;
	rpcproc_t p_procnum;
	SVCXPRT *p_transp;
	char *p_netid;
	char *p_xdrbuf;
	int p_recvsz;
	xdrproc_t p_inproc, p_outproc;
	struct proglst *p_nxt;
} *proglst;

static const char __reg_err1[] = "can't find appropriate transport";
static const char __reg_err2[] = "can't get protocol info";
static const char __reg_err3[] = "unsupported transport size";
static const char __no_mem_str[] = "out of memory";

/*
 * For simplified, easy to use kind of rpc interfaces.
 * nettype indicates the type of transport on which the service will be
 * listening. Used for conservation of the system resource. Only one
 * handle is created for all the services (actually one of each netid)
 * and same xdrbuf is used for same netid. The size of the arguments
 * is also limited by the recvsize for that transport, even if it is
 * a COTS transport. This may be wrong, but for cases like these, they
 * should not use the simplified interfaces like this.
 */

int
rpc_reg(
	rpcprog_t prognum,		/* program number */
	rpcvers_t versnum,		/* version number */
	rpcproc_t procnum,		/* procedure number */
	char *(*progname)(char *),	/* Server routine */
	xdrproc_t inproc,		/* in XDR procedure */
	xdrproc_t outproc,		/* out XDR procedure */
	char *nettype)			/* nettype */
{
	struct netconfig *nconf;
	int done = FALSE;
	void *handle;
#ifdef _REENTRANT
	extern mutex_t proglst_lock;
#endif

	if (procnum == NULLPROC) {
		warnx("%s: can't reassign procedure number %u", __func__,
		    NULLPROC);
		return (-1);
	}

	if (nettype == NULL)
		nettype = __UNCONST("netpath");	/* The default behavior */
	if ((handle = __rpc_setconf(nettype)) == NULL) {
		warnx("%s: %s", __func__, __reg_err1);
		return (-1);
	}
/* VARIABLES PROTECTED BY proglst_lock: proglst */
	mutex_lock(&proglst_lock);
	while ((nconf = __rpc_getconf(handle)) != NULL) {
		struct proglst *pl;
		SVCXPRT *svcxprt;
		int madenow;
		u_int recvsz;
		char *xdrbuf;
		char *netid;

		madenow = FALSE;
		svcxprt = NULL;
		recvsz = 0;
		xdrbuf = NULL;
		netid = NULL;
		for (pl = proglst; pl; pl = pl->p_nxt)
			if (strcmp(pl->p_netid, nconf->nc_netid) == 0) {
				svcxprt = pl->p_transp;
				xdrbuf = pl->p_xdrbuf;
				recvsz = pl->p_recvsz;
				netid = pl->p_netid;
				break;
			}

		if (svcxprt == NULL) {
			struct __rpc_sockinfo si;

			svcxprt = svc_tli_create(RPC_ANYFD, nconf, NULL, 0, 0);
			if (svcxprt == NULL)
				continue;
			if (!__rpc_fd2sockinfo(svcxprt->xp_fd, &si)) {
				warnx("%s: %s", __func__, __reg_err2);
				SVC_DESTROY(svcxprt);
				continue;
			}
			recvsz = __rpc_get_t_size(si.si_af, si.si_proto, 0);
			if (recvsz == 0) {
				warnx("%s: %s", __func__, __reg_err3);
				SVC_DESTROY(svcxprt);
				continue;
			}
			if (((xdrbuf = mem_alloc((size_t)recvsz)) == NULL) ||
				((netid = strdup(nconf->nc_netid)) == NULL)) {
				warnx("%s: %s", __func__, __no_mem_str);
				if (xdrbuf != NULL)
					free(xdrbuf);
				if (netid != NULL)
					free(netid);
				SVC_DESTROY(svcxprt);
				break;
			}
			madenow = TRUE;
		}
		/*
		 * Check if this (program, version, netid) had already been
		 * registered.  The check may save a few RPC calls to rpcbind
		 */
		for (pl = proglst; pl; pl = pl->p_nxt)
			if ((pl->p_prognum == prognum) &&
				(pl->p_versnum == versnum) &&
				(strcmp(pl->p_netid, netid) == 0))
				break;
		if (pl == NULL) { /* Not yet */
			(void) rpcb_unset(prognum, versnum, nconf);
		} else {
			/* so that svc_reg does not call rpcb_set() */
			nconf = NULL;
		}

		if (!svc_reg(svcxprt, prognum, versnum, universal, nconf)) {
			warnx("%s: couldn't register prog %u vers %u for %s",
			    __func__, (unsigned)prognum,
			    (unsigned)versnum, netid);
			if (madenow) {
				SVC_DESTROY(svcxprt);
				free(xdrbuf);
				free(netid);
			}
			continue;
		}

		pl = malloc(sizeof(*pl));
		if (pl == NULL) {
			warn("%s: %s", __func__, __no_mem_str);
			if (madenow) {
				SVC_DESTROY(svcxprt);
				free(xdrbuf);
				free(netid);
			}
			break;
		}
		pl->p_progname = progname;
		pl->p_prognum = prognum;
		pl->p_versnum = versnum;
		pl->p_procnum = procnum;
		pl->p_inproc = inproc;
		pl->p_outproc = outproc;
		pl->p_transp = svcxprt;
		pl->p_xdrbuf = xdrbuf;
		pl->p_recvsz = recvsz;
		pl->p_netid = netid;
		pl->p_nxt = proglst;
		proglst = pl;
		done = TRUE;
	}
	__rpc_endconf(handle);
	mutex_unlock(&proglst_lock);

	if (done == FALSE) {
		warnx("%s: can't find suitable transport for %s",
		    __func__, nettype);
		return (-1);
	}
	return (0);
}

/*
 * The universal handler for the services registered using registerrpc.
 * It handles both the connectionless and the connection oriented cases.
 */

static void
universal(struct svc_req *rqstp, SVCXPRT *transp)
{
	rpcprog_t prog;
	rpcvers_t vers;
	rpcproc_t proc;
	char *outdata;
	char *xdrbuf;
	struct proglst *pl;
#ifdef _REENTRANT
	extern mutex_t proglst_lock;
#endif

	_DIAGASSERT(rqstp != NULL);
	_DIAGASSERT(transp != NULL);

	/*
	 * enforce "procnum 0 is echo" convention
	 */
	if (rqstp->rq_proc == NULLPROC) {
		if (svc_sendreply(transp, (xdrproc_t) xdr_void, NULL) ==
		    FALSE) {
			warnx("%s: svc_sendreply failed", __func__);
		}
		return;
	}
	prog = rqstp->rq_prog;
	vers = rqstp->rq_vers;
	proc = rqstp->rq_proc;
	mutex_lock(&proglst_lock);
	for (pl = proglst; pl; pl = pl->p_nxt)
		if (pl->p_prognum == prog && pl->p_procnum == proc &&
			pl->p_versnum == vers &&
			(strcmp(pl->p_netid, transp->xp_netid) == 0)) {
			/* decode arguments into a CLEAN buffer */
			xdrbuf = pl->p_xdrbuf;
			/* Zero the arguments: reqd ! */
			(void) memset(xdrbuf, 0, (size_t)pl->p_recvsz);
			/*
			 * Assuming that sizeof (xdrbuf) would be enough
			 * for the arguments; if not then the program
			 * may bomb. BEWARE!
			 */
			if (!svc_getargs(transp, pl->p_inproc, xdrbuf)) {
				svcerr_decode(transp);
				mutex_unlock(&proglst_lock);
				return;
			}
			outdata = (*(pl->p_progname))(xdrbuf);
			if (outdata == NULL &&
				pl->p_outproc != (xdrproc_t) xdr_void){
				/* there was an error */
				mutex_unlock(&proglst_lock);
				return;
			}
			if (!svc_sendreply(transp, pl->p_outproc, outdata)) {
				warnx("%s: trouble replying to prog %u vers %u",
				    __func__, (unsigned)prog, (unsigned)vers);
				mutex_unlock(&proglst_lock);
				return;
			}
			/* free the decoded arguments */
			(void) svc_freeargs(transp, pl->p_inproc, xdrbuf);
			mutex_unlock(&proglst_lock);
			return;
		}
	mutex_unlock(&proglst_lock);
	/* This should never happen */
	warnx("%s: never registered prog %u vers %u", __func__,
	    (unsigned)prog, (unsigned)vers);
	return;
}
