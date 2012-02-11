/* $NetBSD: Lint_ntohl.c,v 1.4 2001/08/22 07:42:09 itojun Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <sys/types.h>
#undef ntohl

/*ARGSUSED*/
uint32_t
ntohl(net32)
	uint32_t net32;
{
	return (0);
}
