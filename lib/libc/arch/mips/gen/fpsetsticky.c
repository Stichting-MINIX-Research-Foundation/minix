/*	$NetBSD: fpsetsticky.c,v 1.8 2012/03/20 16:19:56 matt Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetsticky.c,v 1.8 2012/03/20 16:19:56 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetsticky,_fpsetsticky)
#endif

fp_except
fpsetsticky(fp_except sticky)
{
	fp_except old;
	fp_except new;

	__asm("cfc1 %0,$31" : "=r" (old));

	new = old & ~(0x1f << 2); 
	new |= (sticky & 0x1f) << 2;

	__asm("ctc1 %0,$31" : : "r" (new));

	return (old >> 2) & 0x1f;
}
