/*
 * assert.c - diagnostics
 */

#include	<assert.h>
#include	<stdio.h>

#include <sys/types.h>
#include <stdlib.h>
#include <minix/sysutil.h>

void
__assert13(file, line, function, failedexpr)
	const char *file, *function, *failedexpr;
	int line;
{
	(void)printf("%s:%d: assert \"%s\" failed", file, line, failedexpr);
	if(function) printf(", function \"%s\"", function);
	printf("\n");
	panic("assert failed");
	/* NOTREACHED */
}

void
__assert(file, line, failedexpr)
	const char *file, *failedexpr;
	int line;
{

	__assert13(file, line, NULL, failedexpr);
	/* NOTREACHED */
}
