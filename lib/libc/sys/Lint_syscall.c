/* $NetBSD: Lint_syscall.c,v 1.4 2003/01/18 11:32:58 thorpej Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <stdarg.h>
#include <unistd.h>

/*ARGSUSED*/
int
syscall(int arg1, ...)
{
	return (0);
}
