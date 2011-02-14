/* $NetBSD: Lint___clone.c,v 1.2 2003/01/18 11:32:58 thorpej Exp $ */

/*
 * This file placed in the public domain.
 * Jason R. Thorpe, July 16, 2001.
 */

#include <sched.h>

/*ARGSUSED*/
pid_t
__clone(int (*func)(void *), void *stack, int flags, void *arg)
{
	return (0);
}
