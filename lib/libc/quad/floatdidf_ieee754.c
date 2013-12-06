/*	$NetBSD: floatdidf_ieee754.c,v 1.1 2013/08/24 00:51:48 matt Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: floatdidf_ieee754.c,v 1.1 2013/08/24 00:51:48 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(SOFTFLOAT) || defined(__ARM_EABI__)
#include "softfloat/softfloat-for-gcc.h"
#endif

#include <limits.h>
#include <machine/ieee.h>
#include "quad.h"

double __floatdidf(quad_t);

/*
 * Convert signed quad to double.
 */
double
__floatdidf(quad_t x)
{
	union ieee_double_u ux = { .dblu_d = 0.0 };

	if (x == 0)
		return 0.0;
	if (x == 1)
		return 1.0;

	if (x < 0) {
		if (x == QUAD_MIN)
			return -0x1.0p63;
		ux.dblu_sign = 1;
		x = -x;
	}
	u_int l = __builtin_clzll(x);
	x <<= (l + 1);	/* clear implicit bit */
	x >>= 64 - (DBL_FRACHBITS + DBL_FRACLBITS);
	union uu u = { .uq = x };
	ux.dblu_frach = u.ul[H];
	ux.dblu_fracl = u.ul[L];
	ux.dblu_exp = DBL_EXP_BIAS + 63 - l;

	return ux.dblu_d;
}
