/*	$NetBSD: difftime.c,v 1.10 2009/12/31 22:49:16 mlelstv Exp $	*/

/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson.
*/

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char	elsieid[] = "@(#)difftime.c	8.1";
#else
__RCSID("$NetBSD: difftime.c,v 1.10 2009/12/31 22:49:16 mlelstv Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*LINTLIBRARY*/

#include "private.h"	/* for time_t, TYPE_INTEGRAL, and TYPE_SIGNED */

double
difftime(time1, time0)
const time_t	time1;
const time_t	time0;
{
	/*
	** If (sizeof (double) > sizeof (time_t)) simply convert and subtract
	** (assuming that the larger type has more precision).
	** This is the common real-world case circa 2004.
	*/
/* LINTED constant */
	if (sizeof (double) > sizeof (time_t))
		return (double) time1 - (double) time0;
/* LINTED constant */
	if (!TYPE_INTEGRAL(time_t)) {
		/*
		** time_t is floating.
		*/
		return time1 - time0;
	}
/* LINTED constant */
	if (!TYPE_SIGNED(time_t)) {
		/*
		** time_t is integral and unsigned.
		** The difference of two unsigned values can't overflow
		** if the minuend is greater than or equal to the subtrahend.
		*/
		if (time1 >= time0)
			return time1 - time0;
		else	return -((double) (time0 - time1));
	}
	/*
	** time_t is integral and signed.
	** Handle cases where both time1 and time0 have the same sign
	** (meaning that their difference cannot overflow).
	*/
	if ((time1 < 0) == (time0 < 0))
		return time1 - time0;
	/*
	** time1 and time0 have opposite signs.
	** Punt if unsigned long is too narrow.
	*/
/* CONSTCOND */
	if (sizeof (unsigned long) < sizeof (time_t))
		return (double) time1 - (double) time0;
	/*
	** Stay calm...decent optimizers will eliminate the complexity below.
	*/
	if (time1 >= 0 /* && time0 < 0 */)
		return (unsigned long) time1 +
			(unsigned long) (-(time0 + 1)) + 1;
	return -(double) ((unsigned long) time0 +
		(unsigned long) (-(time1 + 1)) + 1);
}

#ifdef __minix
__weak_alias(difftime, __difftime50)
#endif
