/*	$NetBSD: fixunsgen_ieee754.c,v 1.3 2012/03/25 19:53:41 christos Exp $	*/

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

#if !defined(FIXUNSNAME) && defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fixunsgen_ieee754.c,v 1.3 2012/03/25 19:53:41 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

#ifndef FIXUNSNAME
#define	FIXUNSNAME(n)	n##32
#define	UINTXX_T	uint32_t
#endif

__dso_hidden UINTXX_T
	FIXUNSNAME(__fixunsgen)(int, bool, size_t, size_t, const uint32_t *);

/*
 * Convert double to (unsigned) int.  All operations are done module 2^32.
 */
UINTXX_T
FIXUNSNAME(__fixunsgen)(int exp, bool sign, size_t mant_dig, size_t fracbits,
	const uint32_t *frac)
{
	UINTXX_T tmp;

	/*
	 * If it's less than 1 (negative exponent), it's going to round
	 * to zero.  If the exponent is so large that it is a multiple of
	 * 2^N, then x module 2^N will be 0.  (we use the fact treating a
	 * negative value as unsigned will be greater than nonnegative value)
	 */
	if (__predict_false((size_t)exp >= mant_dig + sizeof(UINTXX_T)*8))
		return 0;

	/*
	 * This is simplier than it seems.  Basically we are constructing
	 * fixed binary representation of the floating point number tossing
	 * away bits that wont be in the modulis we return.
	 */
	tmp = 1;
	for (size_t ebits = exp;;) {
		if (ebits <= fracbits) {
			/*
			 * The current fraction has more bits than we need.
			 * Shift the current value over and insert the bits
			 * we want.  We're done.
			 */
			tmp <<= (unsigned int)ebits;
			tmp |= *frac >> (fracbits - ebits);
			break;
		}
		if (fracbits == sizeof(tmp)*4) {
			/*
			 * Shifts must be < sizeof(type).  If it's going to be
			 * sizeof(type), just replace the value.
			 */
			tmp = *frac--;
		} else {
			tmp <<= (unsigned int)fracbits;
			tmp |= *frac--;
		}
		ebits -= fracbits;
		fracbits = sizeof(frac[0]) * 4;
	}

	/*
	 * If the input was negative, make tmp negative module 2^32.
	 */
	if (sign)
		tmp = -tmp;

	return tmp;
}
