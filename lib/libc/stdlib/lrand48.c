/*	$NetBSD: lrand48.c,v 1.9 2013/10/22 08:08:51 matt Exp $	*/

/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.
 *
 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.
 *
 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: lrand48.c,v 1.9 2013/10/22 08:08:51 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "rand48.h"

#ifdef __weak_alias
__weak_alias(lrand48,_lrand48)
#endif

long
lrand48(void)
{
	__dorand48(__rand48_seed);
	return __rand48_seed[2] * 32768 + (__rand48_seed[1] >> 1);
}
