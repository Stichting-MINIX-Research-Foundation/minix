/*	$NetBSD: fpgetround.c,v 1.7 2014/09/17 11:02:55 joerg Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetround.c,v 1.7 2014/09/17 11:02:55 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpgetround,_fpgetround)
#endif

fp_rnd
fpgetround(void)
{
	fp_rnd x;

	__asm(".set push; .set noat; cfc1 %0,$31; .set pop" : "=r" (x));
	return x & 0x03;
}
