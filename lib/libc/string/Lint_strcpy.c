/* $NetBSD: Lint_strcpy.c,v 1.2 2000/06/14 06:49:09 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
char *
strcpy(dst, src)
	char *dst;
	const char *src;
{
	return (0);
}
