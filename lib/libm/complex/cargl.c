/* $NetBSD: cargl.c,v 1.1 2014/10/10 00:48:18 christos Exp $ */

/*
 * Public domain.
 */

#include "../src/namespace.h"
#include <complex.h>
#include <math.h>

long double
cargl(long double complex z)
{

	return atan2l(__imag__ z, __real__ z);
}
