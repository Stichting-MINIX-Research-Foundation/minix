/* $NetBSD: Lint_memset.c,v 1.2 2000/06/14 06:49:08 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
void *
memset(b, c, len)
	void *b;
	int c;
	size_t len;
{
	return (0);
}
