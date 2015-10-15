/*	$NetBSD: fpgetmask.c,v 1.9 2014/09/17 11:02:55 joerg Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetmask.c,v 1.9 2014/09/17 11:02:55 joerg Exp $");
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

	__asm(".set push; .set noat; cfc1 %0,$31; .set pop" : "=r" (x));
	return (x >> 7) & 0x1f;
}
