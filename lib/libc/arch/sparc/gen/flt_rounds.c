/*	$NetBSD: flt_rounds.c,v 1.7 2012/03/21 00:38:34 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: flt_rounds.c,v 1.7 2012/03/21 00:38:34 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <machine/float.h>

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds(void)
{
	unsigned int x;

	__asm("st %%fsr,%0" : "=m" (*&x));
	return map[(x >> 30) & 0x03];
}
