/* $NetBSD: Lint_memchr.c,v 1.2 2000/06/14 06:49:07 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
void *
memchr(b, c, len)
	const void *b;
	int c;
	size_t len;
{
	return (0);
}
