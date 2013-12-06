/* $NetBSD: __aeabi_fcmpgt.c,v 1.2 2013/04/16 13:38:34 matt Exp $ */

/*
 * Written by Ben Harris, 2000.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: __aeabi_fcmpgt.c,v 1.2 2013/04/16 13:38:34 matt Exp $");
#endif /* LIBC_SCCS and not lint */

int __aeabi_fcmpgt(float32, float32);

int
__aeabi_fcmpgt(float32 a, float32 b)
{

	return !float32_le(a, b) && float32_eq(a, a) && float32_eq(b, b);
}
