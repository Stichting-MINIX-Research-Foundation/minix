/* $NetBSD: Lint_ldiv.c,v 1.2 2000/06/14 06:49:07 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <stdlib.h>

/*ARGSUSED*/
ldiv_t
ldiv(num, denom)
	long num, denom;
{
	ldiv_t rv = { 0 };
	return (rv);
}
