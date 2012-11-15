/*	$NetBSD: floatunditf_ieee754.c,v 1.4 2012/08/05 04:28:58 matt Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)floatunsdidf.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: floatunditf_ieee754.c,v 1.4 2012/08/05 04:28:58 matt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#ifdef SOFTFLOAT
#include "softfloat/softfloat-for-gcc.h"
#endif

#include "quad.h"
#ifdef __vax__
#error vax does not support a distinct long double
#endif
#include <machine/ieee.h>

/*
 * Convert (unsigned) quad to long double.
 * This is exactly like floatdidf.c except that negatives never occur.
 */
long double
__floatunditf(u_quad_t x)
{
#if 0
	long double ld;
	union uu u;

	u.uq = x;
	ld = (long double)u.ul[H] * (((int)1 << (INT_BITS - 2)) * 4.0);
	ld += u.ul[L];
	return (ld);
#else
	union ieee_ext_u extu;
	quad_t tmp = x;		/* must be signed */
	unsigned int width = 64;
	unsigned int bit = 0;
	quad_t mask = ~(quad_t)0;

	if (x == 0)
		return 0.0L;
	if (x == 1)
		return 1.0L;

	while (mask != 0 && (tmp >= 0)) {
		width >>= 1;
		mask <<= width;
		if ((tmp & mask) == 0) {
			tmp <<= width;
			bit += width;
		}
	}

	x <<= (bit + 1);
	extu.extu_sign = 0;
	extu.extu_exp = EXT_EXP_BIAS + (64 - (bit + 1));
	extu.extu_frach = (unsigned int)(x >> (64 - EXT_FRACHBITS));
	x <<= EXT_FRACHBITS;
#ifdef EXT_FRACHMBITS
	extu.extu_frachm =(unsigned int)(x >> (64 - EXT_FRACHMBITS));
	x <<= EXT_FRACHMBITS;
#endif
#ifdef EXT_FRACLMBITS
	extu.extu_fraclm =(unsigned int)(x >> (64 - EXT_FRACLMBITS));
	x <<= EXT_FRACLMBITS;
#endif
	extu.extu_fracl =(unsigned int)(x >> (64 - EXT_FRACLBITS));

	return extu.extu_ld;
#endif
}
