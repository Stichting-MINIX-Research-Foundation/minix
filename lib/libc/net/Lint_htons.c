/* $NetBSD: Lint_htons.c,v 1.4 2001/08/22 07:42:09 itojun Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <sys/types.h>
#undef htons

/*ARGSUSED*//*NOSTRICT*/
uint16_t
htons(host16)
	uint16_t host16;
{
	return (0);
}
