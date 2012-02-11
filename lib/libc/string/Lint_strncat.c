/*	$NetBSD: Lint_strncat.c,v 1.1 1997/12/07 00:24:58 matthias Exp $	*/

/*
 * This file placed in the public domain.
 * Matthias Pfaller, December 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
char *
strncat(dst, src, n)
	char *dst;
	const char *src;
	size_t n;
{
	return (0);
}
