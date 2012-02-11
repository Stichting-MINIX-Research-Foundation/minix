/* $NetBSD: catan.c,v 1.1 2007/08/20 16:01:32 drochner Exp $ */

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

#include "../src/namespace.h"
#include <complex.h>
#include <math.h>
#include "cephes_subr.h"

#ifdef __weak_alias
__weak_alias(catan, _catan)
#endif

#define MAXNUM 1.0e308

double complex
catan(double complex z)
{
	double complex w;
	double a, t, x, x2, y;

	x = creal(z);
	y = cimag(z);

	if ((x == 0.0) && (y > 1.0))
		goto ovrf;

	x2 = x * x;
	a = 1.0 - x2 - (y * y);
	if (a == 0.0)
		goto ovrf;

	t = 0.5 * atan2(2.0 * x, a);
	w = _redupi(t);

	t = y - 1.0;
	a = x2 + (t * t);
	if (a == 0.0)
		goto ovrf;

	t = y + 1.0;
	a = (x2 + (t * t))/a;
	w = w + (0.25 * log(a)) * I;
	return w;

ovrf:
#if 0
	mtherr ("catan", OVERFLOW);
#endif
	w = MAXNUM + MAXNUM * I;
	return w;
}
