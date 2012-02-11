/*	$NetBSD: infinityl.c,v 1.2 2005/06/12 05:21:27 lukem Exp $	*/

/*
 * infinityl.c - max. value representable in VAX D_floating  -- public domain.
 * This is _not_ infinity.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: infinityl.c,v 1.2 2005/06/12 05:21:27 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#include <math.h>

const union __long_double_u __infinityl =
	{ { 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };
