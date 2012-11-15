/*	$NetBSD: getdtablesize.c,v 1.10 2012/06/24 15:26:03 christos Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: getdtablesize.c,v 1.10 2012/06/24 15:26:03 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <unistd.h>

int
getdtablesize(void)
{
	return ((int)sysconf(_SC_OPEN_MAX));
}
