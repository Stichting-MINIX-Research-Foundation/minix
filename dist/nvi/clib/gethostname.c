/*	$NetBSD: gethostname.c,v 1.1.1.2 2008/05/18 14:29:38 aymeric Exp $ */

#include "config.h"

/*
 * Solaris doesn't include the gethostname call by default.
 */
#include <sys/utsname.h>
#include <sys/systeminfo.h>

#include <netdb.h>

/*
 * PUBLIC: #ifndef HAVE_GETHOSTNAME
 * PUBLIC: int gethostname __P((char *, int));
 * PUBLIC: #endif
 */
int
gethostname(host, len)
	char *host;
	int len;
{
	return (sysinfo(SI_HOSTNAME, host, len) == -1 ? -1 : 0);
}
