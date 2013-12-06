/* $NetBSD: consttime_memequal.c,v 1.4 2013/08/28 19:31:14 riastradh Exp $ */

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include "namespace.h"
#include <string.h>
#ifdef __weak_alias
__weak_alias(consttime_memequal,_consttime_memequal)
#endif
#else
#include <lib/libkern/libkern.h>
#endif

int
consttime_memequal(const void *b1, const void *b2, size_t len)
{
	const char *c1 = b1, *c2 = b2;
	int res = 0;

	while (len --)
		res |= *c1++ ^ *c2++;

	/*
	 * If the compiler for your favourite architecture generates a
	 * conditional branch for `!res', it will be a data-dependent
	 * branch, in which case this should be replaced by
	 *
	 *	return (1 - (1 & ((res - 1) >> 8)));
	 *
	 * or rewritten in assembly.
	 */
	return !res;
}
