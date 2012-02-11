/*	$NetBSD: infinity_ieee754.c,v 1.3 2005/06/12 05:21:27 lukem Exp $	*/

/*
 * IEEE-compatible infinity.c -- public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: infinity_ieee754.c,v 1.3 2005/06/12 05:21:27 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#include <math.h>
#include <machine/endian.h>

const union __double_u __infinity =
#if BYTE_ORDER == BIG_ENDIAN
	{ { 0x7f, 0xf0, 0, 0, 0, 0,    0,    0 } };
#else
	{ {    0,    0, 0, 0, 0, 0, 0xf0, 0x7f } };
#endif
