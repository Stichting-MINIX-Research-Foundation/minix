/* $NetBSD: Lint___vfork14.c,v 1.3 2012/06/24 15:26:03 christos Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
pid_t
__vfork14(void)
{
	return (0);
}
