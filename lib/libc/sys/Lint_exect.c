/* $NetBSD: Lint_exect.c,v 1.2 2000/06/14 06:49:10 cgd Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
exect(path, argv, envp)
	const char *path;
	char * const * argv;
	char * const * envp;
{
	return (0);
}
