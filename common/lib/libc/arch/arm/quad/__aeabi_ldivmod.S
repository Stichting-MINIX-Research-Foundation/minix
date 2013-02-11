/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include <machine/asm.h>

RCSID("$NetBSD: __aeabi_ldivmod.S,v 1.1 2012/08/05 06:33:51 matt Exp $")

ENTRY(__aeabi_ldivmod)
	push	{r4-r5, sl, lr}
#define	NEG	r5
	mov	NEG, #0
	
#ifdef __ARMEB__
#define	ALO	r1	/* incoming numerator, outgoing quotient */
#define	AHI	r0	/* incoming numerator, outgoing quotient */
#define	BLO	r3	/* incoming denominator, outgoing remainder */
#define	BHI	r2	/* incoming denominator, outgoing remainder */
#else
#define	ALO	r0	/* incoming numerator, outgoing quotient */
#define	AHI	r1	/* incoming numerator, outgoing quotient */
#define	BLO	r2	/* incoming denominator, outgoing remainder */
#define	BHI	r3	/* incoming denominator, outgoing remainder */
#endif

	cmp	BHI, #0
	bge	2f
	eor	NEG, NEG, #1	/* flip quotient sign */
	bl	.Lnegate_b
	bcs	.Lmaxdenom

2:
	cmp	AHI, #0
	/* bge	3f */
	eorlt	NEG, NEG, #3	/* flip quotient sign, flip remainder sign */
	bllt	.Lnegate_a
3:
	/*
	 * Arguments are setup, allocate some stack for the remainder
	 * and call __qdivrem for the heavy lifting.
	 */
	sub	sp, sp, #8
	mov	r4, sp		/* pointer to remainder */
	push	{r4}
	bl	PLT_SYM(__qdivrem)
	add	sp, sp, #4	/* forget pointer to remainder */

	teq	NEG, #0		/* any signs to flip? */
	/*
	 * The quotient is already in the right place and neither value
	 * needs its sign flipped.
	 */
	popeq	{r2-r5, sl, lr}
	RETc(eq)

	pop	{r2, r3}
	tst	NEG, #2		/* does remainder need to be negative? */
	blne	.Lnegate_b
	tst	NEG, #1		/* does quotient need to be negative? */
	blne	.Lnegate_a
	pop	{r4-r5, sl, lr}
	RET

.Lnegate_a:
        rsbs	ALO, ALO, #0
        rsc	AHI, AHI, #0
	RET

.Lnegate_b:
        rsbs	BLO, BLO, #0
        rsc	BHI, BHI, #0
	RET

.Lmaxdenom:
	/*
	 * We had a carry so the denominator must have INT64_MIN
	 * Also BLO and BHI never changed values so we can use
	 * them to see if the numerator has the same value.  We
	 * don't have to worry about sign.
	 */
	teq	BHI, AHI 
	teqeq	BLO, ALO
	bne	1f

	/*
	 * They were equal, so we return a quotient of 1 and remainder of 0.
	 */
	mov	ALO, #1
	mov	AHI, #0
	mov	BLO, #0
	mov	BHI, #0
	pop	{r4-r5, sl, lr}
	RET

	/*
	 * Our remainder must be the numerator and our quotient is 0.
	 */
1:	mov	BLO, ALO
	mov	BHI, AHI
	mov	ALO, #0
	mov	AHI, #0
	pop	{r4-r5, sl, lr}
	RET

END(__aeabi_ldivmod)
