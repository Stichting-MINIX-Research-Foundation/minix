/*	$NetBSD: Lint_getcontext.c,v 1.2 2003/01/18 11:32:58 thorpej Exp $	*/

/*
 * This file placed in the public domain.
 * Klaus Klein, January 26, 1999.
 */

#include <ucontext.h>

/*ARGSUSED*/
int
getcontext(ucp)
	ucontext_t *ucp;
{

	return (0);
}
