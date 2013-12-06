/*-
 * Copyright (c) 2003, Steven G. Kargl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: s_roundl.c,v 1.1 2013/11/11 23:57:34 joerg Exp $");
#if 0
__FBSDID("$FreeBSD: head/lib/msun/src/s_roundl.c 153017 2005-12-02 13:45:06Z bde $");
#endif

#include "namespace.h"
#include <math.h>

#ifdef __HAVE_LONG_DOUBLE

#ifdef __weak_alias
__weak_alias(roundl, _roundl)
#endif

long double
roundl(long double x)
{
	long double t;

	if (!isfinite(x))
		return (x);

	if (x >= 0.0) {
		t = floorl(x);
		if (t - x <= -0.5)
			t += 1.0;
		return (t);
	} else {
		t = floorl(-x);
		if (t + x <= -0.5)
			t += 1.0;
		return (-t);
	}
}

#endif /* __HAVE_LONG_DOUBLE */
