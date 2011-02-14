/*	$NetBSD: Lint_swapcontext.c,v 1.2 2003/01/18 11:38:47 thorpej Exp $	*/

/*
 * This file placed in the public domain.
 * Klaus Klein, November 29, 1998.
 */

#include <ucontext.h>

/*ARGSUSED*/
int
swapcontext(oucp, ucp)
	ucontext_t *oucp;
	const ucontext_t *ucp;
{

	return (0);
}
