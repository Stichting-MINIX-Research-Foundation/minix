/* $NetBSD: Lint_alloca.c,v 1.2 2000/06/14 06:49:05 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <stdlib.h>

/*ARGSUSED*/
void *
alloca(size)
	size_t size;
{
	return (0);
}
