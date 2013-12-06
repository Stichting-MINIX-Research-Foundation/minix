/*	$NetBSD: s_rintl.c,v 1.5 2013/08/21 13:04:44 martin Exp $	*/

/*-
 * Copyright (c) 2008 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if 0
__FBSDID("$FreeBSD: src/lib/msun/src/s_rintl.c,v 1.5 2008/02/22 11:59:05 bde Exp $");
#else
__RCSID("$NetBSD: s_rintl.c,v 1.5 2013/08/21 13:04:44 martin Exp $");
#endif

#include <float.h>
#include <machine/ieee.h>

#include "math.h"
#include "math_private.h"

#ifdef __HAVE_LONG_DOUBLE
static const float
shift[2] = {
#if EXT_FRACBITS == 64
	0x1.0p63, -0x1.0p63
#elif EXT_FRACBITS == 113
	0x1.0p112, -0x1.0p112
#elif EXT_FRACBITS == 112
	0x1.0p111, -0x1.0p111
#else
#error "Unsupported long double format"
#endif
};
static const float zero[2] = { 0.0, -0.0 };

long double
rintl(long double x)
{
	union ieee_ext_u u;
	uint32_t expsign;
	int ex, sign;

	u.extu_ld = x;
	u.extu_ext.ext_frach &= ~0x80000000;
	expsign = u.extu_ext.ext_sign;
	ex = expsign & 0x7fff;

	if (ex >= EXT_EXP_BIAS + EXT_FRACBITS - 1) {
		if (ex == EXT_EXP_BIAS + EXT_FRACBITS)
			return (x + x);	/* Inf, NaN, or unsupported format */
		return (x);		/* finite and already an integer */
	}
	sign = expsign >> 15;

	/*
	 * The following code assumes that intermediate results are
	 * evaluated in long double precision. If they are evaluated in
	 * greater precision, double rounding may occur, and if they are
	 * evaluated in less precision (as on i386), results will be
	 * wildly incorrect.
	 */
	x += shift[sign];
	x -= shift[sign];

	/*
	 * If the result is +-0, then it must have the same sign as x, but
	 * the above calculation doesn't always give this.  Fix up the sign.
	 */
	if (ex < EXT_EXP_BIAS && x == 0.0L)
		return (zero[sign]);

	return (x);
}
#endif
