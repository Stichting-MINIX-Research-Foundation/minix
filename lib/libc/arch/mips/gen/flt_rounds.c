/*	$NetBSD: flt_rounds.c,v 1.9 2015/03/19 21:22:59 joerg Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: flt_rounds.c,v 1.9 2015/03/19 21:22:59 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <machine/float.h>
#include <ieeefp.h>

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds(void)
{
#ifdef SOFTFLOAT_FOR_GCC
	return map[fpgetround()];
#else
	int x;

	__asm(".set push; .set noat; cfc1 %0,$31; .set pop" : "=r" (x));
	return map[x & 0x03];
#endif
}
