/* $NetBSD: csqrt.c,v 1.1 2007/08/20 16:01:37 drochner Exp $ */

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

double complex
csqrt(double complex z)
{
	double complex w;
	double x, y, r, t, scale;

	x = creal (z);
	y = cimag (z);

	if (y == 0.0) {
		if (x == 0.0) {
			w = 0.0 + y * I;
		} else {
			r = fabs(x);
			r = sqrt(r);
			if (x < 0.0) {
				w = 0.0 + r * I;
			} else {
				w = r + y * I;
			}
		}
		return w;
	}
	if (x == 0.0) {
		r = fabs(y);
		r = sqrt(0.5 * r);
		if (y > 0)
			w = r + r * I;
		else
			w = r - r * I;
		return w;
	}
	/* Rescale to avoid internal overflow or underflow.  */
	if ((fabs(x) > 4.0) || (fabs(y) > 4.0)) {
		x *= 0.25;
		y *= 0.25;
		scale = 2.0;
	} else {
#if 1
		x *= 1.8014398509481984e16;  /* 2^54 */
		y *= 1.8014398509481984e16;
		scale = 7.450580596923828125e-9; /* 2^-27 */
#else
		x *= 4.0;
		y *= 4.0;
		scale = 0.5;
#endif
	}
	w = x + y * I;
	r = cabs(w);
	if (x > 0) {
		t = sqrt(0.5 * r + 0.5 * x);
		r = scale * fabs((0.5 * y) / t );
		t *= scale;
	} else {
		r = sqrt(0.5 * r - 0.5 * x);
		t = scale * fabs((0.5 * y) / r);
		r *= scale;
	}
	if (y < 0)
		w = t - r * I;
	else
		w = t + r * I;
	return w;
}
