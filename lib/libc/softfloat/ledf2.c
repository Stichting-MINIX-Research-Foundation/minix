/* $NetBSD: ledf2.c,v 1.1 2000/06/06 08:15:06 bjh21 Exp $ */

/*
 * Written by Ben Harris, 2000.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ledf2.c,v 1.1 2000/06/06 08:15:06 bjh21 Exp $");
#endif /* LIBC_SCCS and not lint */

flag __ledf2(float64, float64);

flag
__ledf2(float64 a, float64 b)
{

#if defined(__minix) && defined(__arm__)
	return float64_le(a, b);
#else
	/* libgcc1.c says 1 - (a <= b) */
	return 1 - float64_le(a, b);
#endif
}
