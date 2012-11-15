/* $NetBSD: Lint_fpgetmask.c,v 1.3 2012/06/24 15:26:03 christos Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <ieeefp.h>

/*ARGSUSED*/
fp_except
fpgetmask(void)
{
	fp_except rv = { 0 };

	return (rv);
}
