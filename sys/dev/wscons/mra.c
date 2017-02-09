/*	$NetBSD: mra.c,v 1.6 2014/03/14 05:03:19 khorben Exp $	*/

/*
 * Copyright (c) 1999 Shin Takemura All rights reserved.
 * Copyright (c) 1999 PocketBSD Project. All rights reserved.
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mra.c,v 1.6 2014/03/14 05:03:19 khorben Exp $");

#include <sys/param.h>
#include <sys/systm.h>

extern int mra_Y_AX1_BX2_C(const int *, int,
			   const int *, int, const int *, int, int, int,
			   int *, int *, int *);

/*
 * multiple regression analysis
 * Y = AX1 + BX2 + C
 */
int
mra_Y_AX1_BX2_C(const int *y, int ys,
		const int *x1, int x1s, const int *x2, int x2s,
		int n, int scale,
		int *a, int *b, int *c)
{
	int i;
	int64_t X1a, X2a, Ya;
	int64_t X1X1s, X2X2s, X1X2s;
	int64_t YYs, X1Ys, X2Ys;
	int64_t S11, S22, S12;
//	int64_t SYY;
	int64_t S1Y, S2Y;
	int64_t A, B, C, M;
#define AA(p, s, i)	(*((const int *)(((const char *)(p)) + (s) * (i))))
#define X1(i)		AA(x1, x1s, i)
#define X2(i)		AA(x2, x2s, i)
#define Y(i)		AA(y, ys, i)

	/*
	 * get avarage and sum
	 */
	X1a = 0;	X2a = 0;	Ya = 0;
	X1X1s = 0;	X2X2s = 0;	X1X2s = 0;
	X1Ys = 0;	X2Ys = 0;	YYs = 0;
	for (i = 0; i < n; i++) {
		X1a += X1(i);
		X2a += X2(i);
		Ya += Y(i);

		X1X1s += X1(i) * X1(i);
		X2X2s += X2(i) * X2(i);
		X1X2s += X1(i) * X2(i);

		X1Ys += X1(i) * Y(i);
		X2Ys += X2(i) * Y(i);
		YYs += Y(i) * Y(i);
	}
	X1a /= n;	X2a /= n;	Ya /= n;

	S11 = X1X1s - n * X1a * X1a;
	S22 = X2X2s - n * X2a * X2a;
	S12 = X1X2s - n * X1a * X2a;

//	SYY = YYs - n * Ya * Ya;
	S1Y = X1Ys - n * X1a * Ya;
	S2Y = X2Ys - n * X2a * Ya;

#if 0
	printf("X1a=%d X2a=%d Ya=%d\n", (int)X1a, (int)X2a, (int)Ya);
	printf("X1X1s=%d X2X2s=%d X1X2s=%d\n", (int)X1X1s, (int)X2X2s, (int)X1X2s);
	printf("X1Ys=%d X2Ys=%d YYs=%d\n", (int)X1Ys, (int)X2Ys, (int)YYs);
	printf("S11=%d S22=%d S12=%d\n", (int)S11, (int)S22, (int)S12);
	printf("SYY=%d S1Y=%d S2Y=%d\n", (int)SYY, (int)S1Y, (int)S2Y);
#endif

	M = (S11 * S22 - S12 * S12);
	if (M == 0) {
		/* error */
		return -1;
	}

	A = (S1Y * S22 - S2Y * S12) * scale / M;
	B = (S2Y * S11 - S1Y * S12) * scale / M;
	C = Ya - (A * X1a + B * X2a) / scale;

#if 0
	printf("A=%d B=%d C=%d\n", (int)A, (int)B, (int)C);
#endif
	*a = A;
	*b = B;
	*c = C;

	return (0);
}
