/* $NetBSD: csqrtf.c,v 1.1 2007/08/20 16:01:37 drochner Exp $ */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software written by Stephen L. Moshier.
 * It is redistributed by the NetBSD Foundation by permission of the author.
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

#include <complex.h>
#include <math.h>

float complex
csqrtf(float complex z)
{
	float complex w;
	float x, y, r, t, scale;

	x = crealf (z);
	y = cimagf (z);

	if (y == 0.0f) {
		if (x < 0.0f) {
			w = 0.0f + sqrtf(-x) * I;
			return w;
		} else if (x == 0.0f) {
			return (0.0f + y * I);
		} else {
			w = sqrtf(x) + y * I;
			return w;
		}
	}

	if (x == 0.0f) {
		r = fabsf(y);
		r = sqrtf(0.5f * r);
		if (y > 0)
			w = r + r * I;
		else
			w = r - r * I;
		return w;
	}

	/* Rescale to avoid internal overflow or underflow.  */
	if ((fabsf(x) > 4.0f) || (fabsf(y) > 4.0f)) {
		x *= 0.25f;
		y *= 0.25f;
		scale = 2.0f;
	} else {
#if 1
		x *= 6.7108864e7f; /* 2^26 */
		y *= 6.7108864e7f;
		scale = 1.220703125e-4f; /* 2^-13 */
#else
		x *= 4.0f;
		y *= 4.0f;
		scale = 0.5f;
#endif
	}
	w = x + y * I;
	r = cabsf(w);
	if( x > 0 ) {
		t = sqrtf(0.5f * r + 0.5f * x);
		r = scale * fabsf((0.5f * y) / t);
		t *= scale;
	} else {
		r = sqrtf(0.5f * r - 0.5f * x);
		t = scale * fabsf((0.5f * y) / r);
		r *= scale;
	}

	if (y < 0)
		w = t - r * I;
	else
		w = t + r * I;
	return w;
}
