/*	$NetBSD: pmap_getport.c,v 1.19 2013/03/11 20:19:29 tron Exp $	*/

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
static char *sccsid = "@(#)pmap_getport.c 1.9 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)pmap_getport.c	2.2 88/08/01 4.0 RPCSRC";
#else
__RCSID("$NetBSD: pmap_getport.c,v 1.19 2013/03/11 20:19:29 tron Exp $");
#endif
#endif

/*
 * pmap_getport.c
 * Client interface to pmap rpc service.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include "namespace.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <assert.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

#ifdef __weak_alias
__weak_alias(pmap_getport,_pmap_getport)
#endif

static const struct timeval timeout = { 5, 0 };
static const struct timeval tottimeout = { 60, 0 };

/*
 * Find the mapped port for program,version.
 * Calls the pmap service remotely to do the lookup.
 * Returns 0 if no map exists.
 */

static void
remote_pmap_getport(CLIENT *client, struct pmap *parms, u_short *port)
{
	if (CLNT_CALL(client, (rpcproc_t)PMAPPROC_GETPORT, (xdrproc_t)xdr_pmap,
	    parms, (xdrproc_t)xdr_u_short, port, tottimeout) != RPC_SUCCESS) {
		rpc_createerr.cf_stat = RPC_PMAPFAILURE;
		clnt_geterr(client, &rpc_createerr.cf_error);
	} else if (*port == 0) {
		rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		clnt_geterr(client, &rpc_createerr.cf_error);
	}
	CLNT_DESTROY(client);
}

static CLIENT *
get_client(struct sockaddr_in *address, int tcp)
{
	int sock = -1;
	if (tcp)
		return clnttcp_create(address, PMAPPROG, PMAPVERS, &sock, 0, 0);
	else
		return clntudp_bufcreate(address, PMAPPROG, PMAPVERS, timeout,
		    &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
}

u_short
pmap_getport(struct sockaddr_in *address, u_long program, u_long version,
    u_int protocol)
{
	u_short port = 0;
	CLIENT *client;
	struct pmap parms;

	_DIAGASSERT(address != NULL);

	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_prot = protocol;
	parms.pm_port = 0;  /* not needed or used */

	address->sin_port = htons(PMAPPORT);

	client = get_client(address, protocol == IPPROTO_TCP);
	if (client != NULL)
		remote_pmap_getport(client, &parms, &port);

	if (port == 0) {
		client = get_client(address, protocol != IPPROTO_TCP);
		if (client != NULL)
			remote_pmap_getport(client, &parms, &port);
	}

	address->sin_port = 0;
	return port;
}
