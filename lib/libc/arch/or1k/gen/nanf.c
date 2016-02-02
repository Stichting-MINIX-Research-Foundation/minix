/*	$NetBSD: nanf.c,v 1.1 2014/09/03 19:34:25 matt Exp $	*/

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: nanf.c,v 1.1 2014/09/03 19:34:25 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include <math.h>
#include <machine/endian.h>

/* bytes for quiet NaN (IEEE single precision) */
const union __float_u __nanf =
		{ { 0x7f, 0xc0,    0,    0 } };

__warn_references(__nanf, "warning: <math.h> defines NAN incorrectly for your compiler.")
