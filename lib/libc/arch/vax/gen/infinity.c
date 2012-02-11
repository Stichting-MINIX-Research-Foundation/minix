/*	$NetBSD: infinity.c,v 1.8 2002/02/19 21:50:01 thorpej Exp $	*/

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: infinity.c,v 1.8 2002/02/19 21:50:01 thorpej Exp $");
#endif /* LIBC_SCCS and not lint */
/*
 * XXX - This is not correct, but what can we do about it???
 */

/* infinity.c */

#include <math.h>

/* The highest D float on a vax. */
const union __double_u __infinity =
	{ { 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };
