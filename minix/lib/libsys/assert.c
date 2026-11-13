/*
 * assert.c - diagnostics
 */

#include	<assert.h>
#include	<stdio.h>

#include <sys/types.h>
#include <stdlib.h>
#include <minix/sysutil.h>

void __assert13(const char *file, int line, const char *function, const char *failedexpr)
{
	if (function != NULL)
		panic("%s:%d: assert \"%s\" failed, function \"%s\"",
		    file, line, failedexpr, function);
	else
		panic("%s:%d: assert \"%s\" failed", file, line,
		    failedexpr);
	/* NOTREACHED */
}

void __assert(const char *file, int line, const char *failedexpr)
{

	__assert13(file, line, NULL, failedexpr);
	/* NOTREACHED */
}
