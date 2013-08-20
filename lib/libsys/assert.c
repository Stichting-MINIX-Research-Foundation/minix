/*
 * assert.c - diagnostics
 */

#include	<assert.h>
#include	<stdio.h>

#include <sys/types.h>
#include <stdlib.h>
#include <minix/sysutil.h>

void
__assert13(const char *file, int line, const char *function, const char *failedexpr)
{
	(void)printf("%s:%d: assert \"%s\" failed", file, line, failedexpr);
	if(function) printf(", function \"%s\"", function);
	printf("\n");
	panic("assert failed");
	/* NOTREACHED */
}

void
__assert(const char *file, int line, const char *failedexpr)
{

	__assert13(file, line, NULL, failedexpr);
	/* NOTREACHED */
}
