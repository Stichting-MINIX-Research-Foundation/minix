/* $NetBSD: explicit_memset.c,v 1.3 2013/08/28 17:47:07 riastradh Exp $ */

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include "namespace.h"
#include <string.h>
#ifdef __weak_alias
__weak_alias(explicit_memset,_explicit_memset)
#endif
#define explicit_memset_impl __explicit_memset_impl
#else
#include <lib/libkern/libkern.h>
#endif

/*
 * The use of a volatile pointer guarantees that the compiler
 * will not optimise the call away.
 */
void *(* volatile explicit_memset_impl)(void *, int, size_t) = memset;

void *
explicit_memset(void *b, int c, size_t len)
{

	return (*explicit_memset_impl)(b, c, len);
}
