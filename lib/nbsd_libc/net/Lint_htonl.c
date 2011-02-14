/* $NetBSD: Lint_htonl.c,v 1.4 2001/08/22 07:42:08 itojun Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <sys/types.h>
#undef htonl

/*ARGSUSED*/
uint32_t
htonl(host32)
	uint32_t host32;
{
	return (0);
}
