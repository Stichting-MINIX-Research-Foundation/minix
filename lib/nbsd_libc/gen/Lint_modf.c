/* $NetBSD: Lint_modf.c,v 1.2 2000/06/14 06:49:05 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <math.h>

/*ARGSUSED*/
double
modf(value, iptr)
	double value, *iptr;
{
	return (0.0);
}
