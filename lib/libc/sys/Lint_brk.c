/* $NetBSD: Lint_brk.c,v 1.3 2000/06/14 06:49:10 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
brk(addr)
	void *addr;
{
	return (0);
}
