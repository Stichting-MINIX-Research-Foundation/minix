/*	$NetBSD: fpgetmask.c,v 1.8 2012/03/20 16:19:56 matt Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetmask.c,v 1.8 2012/03/20 16:19:56 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpgetmask,_fpgetmask)
#endif

fp_except
fpgetmask(void)
{
	fp_except x;

	__asm("cfc1 %0,$31" : "=r" (x));
	return (x >> 7) & 0x1f;
}
