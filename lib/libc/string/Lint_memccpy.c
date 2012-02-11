/*	$NetBSD: Lint_memccpy.c,v 1.1 1997/12/07 00:24:58 matthias Exp $	*/

/*
 * This file placed in the public domain.
 * Matthias Pfaller, December 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
void *
memccpy(dst, src, c, n)
	void *dst;
	const void *src;
	int c;
	size_t n;
{
	return(0);
}
