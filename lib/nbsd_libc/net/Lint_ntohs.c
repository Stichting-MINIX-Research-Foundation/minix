/* $NetBSD: Lint_ntohs.c,v 1.4 2001/08/22 07:42:09 itojun Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <sys/types.h>
#undef ntohs

/*ARGSUSED*//*NOSTRICT*/
uint16_t
ntohs(net16)
	uint16_t net16;
{
	return (0);
}
