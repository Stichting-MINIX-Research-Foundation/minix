/*	$NetBSD: fpsetmask.c,v 1.8 2012/03/20 16:19:56 matt Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetmask.c,v 1.8 2012/03/20 16:19:56 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetmask,_fpsetmask)
#endif

fp_except
fpsetmask(fp_except mask)
{
	fp_except old;
	fp_except new;

	__asm("cfc1 %0,$31" : "=r" (old));

	new = old & ~(0x1f << 7); 
	new |= ((mask & 0x1f) << 7);

	__asm("ctc1 %0,$31" : : "r" (new));

	return (old >> 7) & 0x1f;
}
