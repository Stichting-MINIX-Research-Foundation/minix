/*	$NetBSD: pmap_getmaps.c,v 1.17 2012/03/20 17:14:50 matt Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)pmap_getmaps.c 1.10 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)pmap_getmaps.c	2.2 88/08/01 4.0 RPCSRC";
#else
__RCSID("$NetBSD: pmap_getmaps.c,v 1.17 2012/03/20 17:14:50 matt Exp $");
#endif
#endif

/*
 * pmap_getmap.c
 * Client interface to pmap rpc service.
 * contains pmap_getmaps, which is only tcp service involved
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include "namespace.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

#ifdef __weak_alias
__weak_alias(pmap_getmaps,_pmap_getmaps)
#endif

#define NAMELEN 255
#define MAX_BROADCAST_SIZE 1400

/*
 * Get a copy of the current port maps.
 * Calls the pmap service remotely to do get the maps.
 */
struct pmaplist *
pmap_getmaps(struct sockaddr_in *address)
{
	struct pmaplist *head = NULL;
	int sock = -1;
	struct timeval minutetimeout;
	CLIENT *client;

	_DIAGASSERT(address != NULL);

	minutetimeout.tv_sec = 60;
	minutetimeout.tv_usec = 0;
	address->sin_port = htons(PMAPPORT);
	client = clnttcp_create(address, PMAPPROG,
	    PMAPVERS, &sock, 50, 500);
	if (client != NULL) {
		if (CLNT_CALL(client, (rpcproc_t)PMAPPROC_DUMP,
		    (xdrproc_t)xdr_void, NULL,
		    (xdrproc_t)xdr_pmaplist, &head, minutetimeout) !=
		    RPC_SUCCESS) {
			clnt_perror(client, "pmap_getmaps rpc problem");
		}
		CLNT_DESTROY(client);
	}
	address->sin_port = 0;
	return (head);
}
