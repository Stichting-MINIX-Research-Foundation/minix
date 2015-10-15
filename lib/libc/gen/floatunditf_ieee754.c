/*	$NetBSD: floatunditf_ieee754.c,v 1.1 2014/01/30 15:06:18 joerg Exp $	*/

/*-
 * Copyright (c) 2014 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: floatunditf_ieee754.c,v 1.1 2014/01/30 15:06:18 joerg Exp $");

#include <math.h>
#include <machine/ieee.h>
#include <limits.h>

#ifdef __HAVE_LONG_DOUBLE

long double __floatunditf(uint64_t x);

/*
 * Convert uint64_t to long double.
 */
long double
__floatunditf(uint64_t x)
{
	int exponent, zeros;
	union ieee_ext_u ux;

	/* Use the easier conversion routine for uint32_t if possible. */
	if (x <= UINT32_MAX)
		return (long double)(uint32_t)x;

	zeros = __builtin_clzll(x);
#ifdef LDBL_IMPLICIT_NBIT
	exponent = 64 - zeros;
	x <<= zeros + 1;
#else
	exponent = 63 - zeros;
	x <<= zeros;
#endif

	ux.extu_exp = EXT_EXP_BIAS + exponent;
	ux.extu_frach = (x >> (64 - EXT_FRACHBITS));
	x <<= EXT_FRACHBITS;
#ifdef EXT_FRACHMBITS
	ux.extu_frachm = (x >> (64 - EXT_FRACHMBITS));
	x <<= EXT_FRACHMBITS;
#endif
#ifdef EXT_FRACLMBITS
	ux.extu_fraclm = (x >> (64 - EXT_FRACLMBITS));
	x <<= EXT_FRACLMBITS;
#endif
	ux.extu_fracl = (x >> (64 - EXT_FRACLBITS));
	ux.extu_sign = 0;

	return ux.extu_ld;
}
#endif /* __HAVE_LONG_DOUBLE */
