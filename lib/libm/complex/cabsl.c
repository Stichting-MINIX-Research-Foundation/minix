/* $NetBSD: cabsl.c,v 1.1 2014/10/10 00:48:18 christos Exp $ */

/*
 * Public domain.
 */

#include "../src/namespace.h"
#include <complex.h>
#include <math.h>

long double
cabsl(long double complex z)
{

	return hypotl(__real__ z, __imag__ z);
}
