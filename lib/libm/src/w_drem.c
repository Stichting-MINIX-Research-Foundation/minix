/*
 * drem() wrapper for remainder().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <sys/cdefs.h>
#if defined(LIBM_SCCS) && !defined(lint)
__RCSID("$NetBSD: w_drem.c,v 1.4 2004/06/25 15:57:38 drochner Exp $");
#endif

#include <math.h>

double
drem(double x, double y)
{
	return remainder(x, y);
}
