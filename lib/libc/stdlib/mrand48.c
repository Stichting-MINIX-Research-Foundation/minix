/*	$NetBSD: mrand48.c,v 1.8 2013/10/22 08:08:51 matt Exp $	*/

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
__RCSID("$NetBSD: mrand48.c,v 1.8 2013/10/22 08:08:51 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "rand48.h"

#ifdef __weak_alias
__weak_alias(mrand48,_mrand48)
#endif

long
mrand48(void)
{
	__dorand48(__rand48_seed);
	return (int16_t)__rand48_seed[2] * 65536 + __rand48_seed[1];
}
