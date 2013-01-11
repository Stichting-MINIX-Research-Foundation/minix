/*	$NetBSD: getrpcport.c,v 1.17 2012/03/20 17:14:50 matt Exp $	*/

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
static char *sccsid = "@(#)getrpcport.c 1.3 87/08/11 SMI";
static char *sccsid = "@(#)getrpcport.c	2.1 88/07/29 4.0 RPCSRC";
#else
__RCSID("$NetBSD: getrpcport.c,v 1.17 2012/03/20 17:14:50 matt Exp $");
#endif
#endif

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include "namespace.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#ifdef __weak_alias
__weak_alias(getrpcport,_getrpcport)
#endif

int
getrpcport(char *host, int prognum, int versnum, int proto)
{
	struct sockaddr_in addr;
	struct hostent *hp;

	_DIAGASSERT(host != NULL);

	if ((hp = gethostbyname(host)) == NULL)
		return (0);
	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(struct sockaddr_in);
	addr.sin_family = AF_INET;
	addr.sin_port =  0;
	if (hp->h_length > addr.sin_len)
		hp->h_length = addr.sin_len;
	memcpy(&addr.sin_addr.s_addr, hp->h_addr, (size_t)hp->h_length);
	/* Inconsistent interfaces need casts! :-( */
	return (pmap_getport(&addr, (u_long)prognum, (u_long)versnum, 
	    (u_int)proto));
}
