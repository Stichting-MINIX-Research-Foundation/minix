/*	$NetBSD: svc_run.c,v 1.20 2012/06/24 15:26:03 christos Exp $	*/

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
static char *sccsid = "@(#)svc_run.c 1.1 87/10/13 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc_run.c	2.1 88/07/29 4.0 RPCSRC";
#else
__RCSID("$NetBSD: svc_run.c,v 1.20 2012/06/24 15:26:03 christos Exp $");
#endif
#endif

/*
 * This is the rpc server side idle loop
 * Wait for input, call server program.
 */
#include "namespace.h"
#include "reentrant.h"
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rpc/rpc.h>

#include "rpc_internal.h"

#ifdef __weak_alias
__weak_alias(svc_run,_svc_run)
__weak_alias(svc_exit,_svc_exit)
#endif

void
svc_run(void)
{
	fd_set readfds, cleanfds;
	struct timeval timeout;
#ifdef _REENTRANT
	extern rwlock_t svc_fd_lock;
#endif

	timeout.tv_sec = 30;
	timeout.tv_usec = 0;

	for (;;) {
		rwlock_rdlock(&svc_fd_lock);
		readfds = svc_fdset;
		cleanfds = svc_fdset;
		rwlock_unlock(&svc_fd_lock);
		switch (select(svc_maxfd+1, &readfds, NULL, NULL, &timeout)) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			warn("svc_run: - select failed");
			return;
		case 0:
			__svc_clean_idle(&cleanfds, 30, FALSE);
			continue;
		default:
			svc_getreqset(&readfds);
		}
	}
}

/*
 *      This function causes svc_run() to exit by telling it that it has no
 *      more work to do.
 */
void
svc_exit(void)
{
#ifdef _REENTRANT
	extern rwlock_t svc_fd_lock;
#endif

	rwlock_wrlock(&svc_fd_lock);
	FD_ZERO(&svc_fdset);
	rwlock_unlock(&svc_fd_lock);
}
