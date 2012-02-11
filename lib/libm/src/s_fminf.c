/*-
 * Copyright (c) 2004 David Schultz <das@FreeBSD.ORG>
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
__RCSID("$NetBSD: s_fminf.c,v 1.2 2010/03/08 01:05:20 snj Exp $");
#ifdef notdef
__FBSDID("$FreeBSD: src/lib/msun/src/s_fminf.c,v 1.1 2004/06/30 07:04:01 das Exp $");
#endif

#include <math.h>

#include <machine/ieee.h>

float
fminf(float x, float y)
{
	union ieee_single_u u[2];

	u[0].sngu_f = x;
	u[1].sngu_f = y;

	/* Check for NaNs to avoid raising spurious exceptions. */
	if (u[0].sngu_sng.sng_exp == SNG_EXP_INFNAN &&
	    u[0].sngu_sng.sng_frac != 0)
		return (y);
	if (u[1].sngu_sng.sng_exp == SNG_EXP_INFNAN &&
	    u[1].sngu_sng.sng_frac != 0)
		return (x);

	/* Handle comparisons of sng_singed zeroes. */
	if (u[0].sngu_sng.sng_sign != u[1].sngu_sng.sng_sign)
		return (u[u[1].sngu_sng.sng_sign].sngu_f);

	return (x < y ? x : y);
}
