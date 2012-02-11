/* $NetBSD: Lint___syscall.c,v 1.3 2003/01/18 11:32:58 thorpej Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <stdarg.h>
#include <unistd.h>

/*ARGSUSED*/
quad_t
__syscall(quad_t arg1, ...)
{
	return (0);
}
