/* $NetBSD: Lint_fpsetround.c,v 1.2 2000/06/14 06:49:05 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <ieeefp.h>

/*ARGSUSED*/
fp_rnd
fpsetround(r)
	fp_rnd r;
{
	fp_rnd rv = { 0 };

	return (rv);
}
