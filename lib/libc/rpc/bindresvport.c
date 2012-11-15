/*	$NetBSD: bindresvport.c,v 1.23 2012/03/20 17:14:50 matt Exp $	*/

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
static char *sccsid = "@(#)bindresvport.c 1.8 88/02/08 SMI";
static char *sccsid = "@(#)bindresvport.c	2.2 88/07/29 4.0 RPCSRC";
#else
__RCSID("$NetBSD: bindresvport.c,v 1.23 2012/03/20 17:14:50 matt Exp $");
#endif
#endif

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#include "namespace.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <rpc/rpc.h>

#ifdef __weak_alias
__weak_alias(bindresvport,_bindresvport)
__weak_alias(bindresvport_sa,_bindresvport_sa)
#endif

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport(int sd, struct sockaddr_in *brsin)
{
	return bindresvport_sa(sd, (struct sockaddr *)(void *)brsin);
}

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport_sa(int sd, struct sockaddr *sa)
{
	int error, old;
	struct sockaddr_storage myaddr;
	struct sockaddr_in *brsin;
#ifdef INET6
	struct sockaddr_in6 *brsin6;
#endif
	int proto, portrange, portlow;
	u_int16_t *portp;
	socklen_t salen;
	int af;

	if (sa == NULL) {
		salen = sizeof(myaddr);
		sa = (struct sockaddr *)(void *)&myaddr;

		if (getsockname(sd, sa, &salen) == -1)
			return -1;	/* errno is correctly set */

		af = sa->sa_family;
		memset(sa, 0, salen);
	} else
		af = sa->sa_family;

	switch (af) {
	case AF_INET:
		proto = IPPROTO_IP;
		portrange = IP_PORTRANGE;
		portlow = IP_PORTRANGE_LOW;
		brsin = (struct sockaddr_in *)(void *)sa;
		salen = sizeof(struct sockaddr_in);
		portp = &brsin->sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		proto = IPPROTO_IPV6;
		portrange = IPV6_PORTRANGE;
		portlow = IPV6_PORTRANGE_LOW;
		brsin6 = (struct sockaddr_in6 *)(void *)sa;
		salen = sizeof(struct sockaddr_in6);
		portp = &brsin6->sin6_port;
		break;
#endif
	default:
		errno = EPFNOSUPPORT;
		return (-1);
	}
	sa->sa_family = af;
	sa->sa_len = salen;

	if (*portp == 0) {
		socklen_t oldlen = sizeof(old);

		error = getsockopt(sd, proto, portrange, &old, &oldlen);
		if (error < 0)
			return (error);
		error = setsockopt(sd, proto, portrange, &portlow,
		    (socklen_t)sizeof(portlow));
		if (error < 0)
			return (error);
	}

	error = bind(sd, sa, salen);

	if (*portp == 0) {
		int saved_errno = errno;

		if (error < 0) {
			if (setsockopt(sd, proto, portrange, &old,
			    (socklen_t)sizeof(old)) < 0)
				errno = saved_errno;
			return (error);
		}

		if (sa != (struct sockaddr *)(void *)&myaddr) {
			/* What did the kernel assign? */
			if (getsockname(sd, sa, &salen) < 0)
				errno = saved_errno;
			return (error);
		}
	}
	return (error);
}
