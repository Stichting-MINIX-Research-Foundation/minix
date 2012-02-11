/* $NetBSD: Lint_memmove.c,v 1.2 2000/06/14 06:49:08 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
void *
memmove(dst, src, len)
	void *dst;
	const void *src;
	size_t len;
{
	return (0);
}
