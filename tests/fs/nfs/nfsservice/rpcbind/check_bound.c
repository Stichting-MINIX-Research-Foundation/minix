/*	$NetBSD: check_bound.c,v 1.1 2010/07/26 15:53:00 pooka Exp $	*/

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
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/* #ident	"@(#)check_bound.c	1.15	93/07/05 SMI" */

#if 0
#ifndef lint
static	char sccsid[] = "@(#)check_bound.c 1.11 89/04/21 Copyr 1989 Sun Micro";
#endif
#endif

/*
 * check_bound.c
 * Checks to see whether the program is still bound to the
 * claimed address and returns the univeral merged address
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <stdio.h>
#include <netconfig.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include "rpcbind.h"

struct fdlist {
	int fd;
	struct netconfig *nconf;
	struct fdlist *next;
	int check_binding;
};

static struct fdlist *fdhead;	/* Link list of the check fd's */
static struct fdlist *fdtail;
static const char emptystring[] = "";

static bool_t check_bound(struct fdlist *, const char *uaddr);

/*
 * Returns 1 if the given address is bound for the given addr & transport
 * For all error cases, we assume that the address is bound
 * Returns 0 for success.
 */
static bool_t
check_bound(struct fdlist *fdl, const char *uaddr)
{
	int fd;
	struct netbuf *na;
	int ans;

	if (fdl->check_binding == FALSE)
		return (TRUE);

	na = uaddr2taddr(fdl->nconf, uaddr);
	if (!na)
		return (TRUE); /* punt, should never happen */

	fd = __rpc_nconf2fd(fdl->nconf);
	if (fd < 0) {
		free(na);
		return (TRUE);
	}

	ans = bind(fd, (struct sockaddr *)na->buf, na->len);

	rump_sys_close(fd);
	free(na);

	return (ans == 0 ? FALSE : TRUE);
}

int
add_bndlist(struct netconfig *nconf, struct netbuf *baddr)
{
	struct fdlist *fdl;
	struct netconfig *newnconf;

	newnconf = getnetconfigent(nconf->nc_netid);
	if (newnconf == NULL)
		return (-1);
	fdl = (struct fdlist *)malloc((u_int)sizeof (struct fdlist));
	if (fdl == NULL) {
		freenetconfigent(newnconf);
		syslog(LOG_ERR, "no memory!");
		return (-1);
	}
	fdl->nconf = newnconf;
	fdl->next = NULL;
	if (fdhead == NULL) {
		fdhead = fdl;
		fdtail = fdl;
	} else {
		fdtail->next = fdl;
		fdtail = fdl;
	}
	/* XXX no bound checking for now */
	fdl->check_binding = FALSE;

	return 0;
}

bool_t
is_bound(const char *netid, const char *uaddr)
{
	struct fdlist *fdl;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (TRUE);
	return (check_bound(fdl, uaddr));
}

/*
 * Returns NULL if there was some system error.
 * Returns "" if the address was not bound, i.e the server crashed.
 * Returns the merged address otherwise.
 */
char *
mergeaddr(SVCXPRT *xprt, char *netid, char *uaddr, char *saddr)
{
	struct fdlist *fdl;
	char *c_uaddr, *s_uaddr, *m_uaddr, *allocated_uaddr = NULL;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (NULL);
	if (check_bound(fdl, uaddr) == FALSE)
		/* that server died */
		return strdup(emptystring);
	/*
	 * If saddr is not NULL, the remote client may have included the
	 * address by which it contacted us.  Use that for the "client" uaddr,
	 * otherwise use the info from the SVCXPRT.
	 */
	if (saddr != NULL) {
		c_uaddr = saddr;
	} else {
		c_uaddr = taddr2uaddr(fdl->nconf, svc_getrpccaller(xprt));
		if (c_uaddr == NULL) {
			syslog(LOG_ERR, "taddr2uaddr failed for %s",
				fdl->nconf->nc_netid);
			return (NULL);
		}
		allocated_uaddr = c_uaddr;
	}

#ifdef RPCBIND_DEBUG
	if (debugging) {
		if (saddr == NULL) {
			fprintf(stderr, "mergeaddr: client uaddr = %s\n",
			    c_uaddr);
		} else {
			fprintf(stderr, "mergeaddr: contact uaddr = %s\n",
			    c_uaddr);
		}
	}
#endif
	s_uaddr = uaddr;
	/*
	 * This is all we should need for IP 4 and 6
	 */
	m_uaddr = addrmerge(svc_getrpccaller(xprt), s_uaddr, c_uaddr, netid);
#ifdef RPCBIND_DEBUG
	if (debugging)
		fprintf(stderr, "mergeaddr: uaddr = %s, merged uaddr = %s\n",
				uaddr, m_uaddr);
#endif
	if (allocated_uaddr != NULL)
		free(allocated_uaddr);
	return (m_uaddr);
}

/*
 * Returns a netconf structure from its internal list.  This
 * structure should not be freed.
 */
struct netconfig *
rpcbind_get_conf(const char *netid)
{
	struct fdlist *fdl;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (NULL);
	return (fdl->nconf);
}
