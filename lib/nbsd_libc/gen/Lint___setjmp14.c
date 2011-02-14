/* $NetBSD: Lint___setjmp14.c,v 1.3 2005/04/09 12:48:58 dsl Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <setjmp.h>

/*ARGSUSED*/
int
__setjmp14(jmp_buf env)
{
	return 0;
}

/*ARGSUSED*/
void
__longjmp14(jmp_buf env, int val)
{
}
