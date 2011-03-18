/*
 * cabsf() wrapper for hypotf().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <sys/cdefs.h>
#if defined(LIBM_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_cabsf.c,v 1.2 2007/08/10 21:20:35 drochner Exp $");
#endif

#include "../src/namespace.h"
#include <math.h>

struct complex {
	float x;
	float y;
};

float cabsf __P((struct complex));
__warn_references(cabsf, "warning: reference to compatibility cabsf()");

float
cabsf(struct complex z)
{

	return hypotf(z.x, z.y);
}
