/* $NetBSD: Lint_bcopy.c,v 1.2 2000/06/14 06:49:07 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
void
bcopy(src, dst, len)
	const void *src;
	void *dst;
	size_t len;
{
}
