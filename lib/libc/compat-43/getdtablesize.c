/*	$NetBSD: getdtablesize.c,v 1.9 2003/07/26 19:24:41 salo Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: getdtablesize.c,v 1.9 2003/07/26 19:24:41 salo Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <unistd.h>

int
getdtablesize()
{
	return ((int)sysconf(_SC_OPEN_MAX));
}
