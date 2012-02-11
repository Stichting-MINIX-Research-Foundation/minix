/* $NetBSD: creal.c,v 1.2 2010/09/15 16:11:29 christos Exp $ */

/*
 * Written by Matthias Drochner <drochner@NetBSD.org>.
 * Public domain.
 */

#include <complex.h>
#include "../src/math_private.h"

double
creal(double complex z)
{
	double_complex w = { .z = z };

	return (REAL_PART(w));
}
