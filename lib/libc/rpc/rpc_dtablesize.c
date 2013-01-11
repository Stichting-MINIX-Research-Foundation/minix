/*	$NetBSD: rpc_dtablesize.c,v 1.15 2012/03/20 17:14:50 matt Exp $	*/

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
static char *sccsid = "@(#)rpc_dtablesize.c 1.2 87/08/11 Copyr 1987 Sun Micro";
static char *sccsid = "@(#)rpc_dtablesize.c	2.1 88/07/29 4.0 RPCSRC";
#else
__RCSID("$NetBSD: rpc_dtablesize.c,v 1.15 2012/03/20 17:14:50 matt Exp $");
#endif
#endif

#include "namespace.h"

#include <unistd.h>

int _rpc_dtablesize(void);	/* XXX */

/*
 * Cache the result of sysconf(_SC_OPEN_MAX), so we don't have to do an
 * expensive system call every time.
 */
int
_rpc_dtablesize(void)
{
	static int size;
	if (size == 0)
		size = (int)sysconf(_SC_OPEN_MAX);
	return (size);
}
