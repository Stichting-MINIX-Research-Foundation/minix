/*	$NetBSD: flt_rounds.c,v 1.7 2012/06/24 15:26:02 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: flt_rounds.c,v 1.7 2012/06/24 15:26:02 christos Exp $");
#endif /* LIBC_SCCS and not lint */

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

	__asm("cfc1\t%0,$31" : "=r" (x));
	return map[x & 0x03];
#endif
}
