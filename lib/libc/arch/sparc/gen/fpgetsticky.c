/*	$NetBSD: fpgetsticky.c,v 1.6 2012/03/21 00:38:34 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetsticky.c,v 1.6 2012/03/21 00:38:34 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpgetsticky,_fpgetsticky)
#endif

fp_except
fpgetsticky(void)
{
	unsigned int x;

	__asm("st %%fsr,%0" : "=m" (*&x));
	return (x >> 5) & 0x1f;
}
