/* $NetBSD: Lint_sbrk.c,v 1.3 2000/06/14 06:49:10 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
void *
sbrk(incr)
	intptr_t incr;
{
	return (0);
}
