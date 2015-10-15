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

RCSID("$NetBSD: __aeabi_ldivmod.S,v 1.13 2014/05/06 16:02:11 joerg Exp $")

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

ENTRY(__aeabi_ldivmod)
#ifdef __ARM_EABI__
# if !defined(__ARM_DWARF_EH__)
	.fnstart
# endif
	.cfi_startproc
#endif
#if !defined(_KERNEL) && !defined(_STANDALONE)
#if !defined(__thumb__)
	orrs	ip, BLO, BHI
	beq	.Ldivbyzero
#elif defined(_ARM_ARCH_T2)
	cbnz	BLO, 1f
	cbz	BHI, .Ldivbyzero
#else
	cmp	BLO, #0
	bne	1f
	cmp	BHI, #0
	beq	.Ldivbyzero
#endif
1:
#endif

	push	{r4-r6, lr}
#ifdef __ARM_EABI__
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -4
	.cfi_offset 6, -8
	.cfi_offset 5, -12
	.cfi_offset 4, -16
#endif
#define	NEG	r5
	movs	NEG, #0

	cmp	BHI, #0
	bge	2f
	movs	NEG, #1		/* flip quotient sign */
	bl	.Lnegate_b
	bcs	.Lmaxdenom

2:
	cmp	AHI, #0
#ifdef __thumb__
	bge	3f
	movs	r4, #3
	eors	NEG, NEG, r4	/* flip quotient sign, flip remainder sign */
	bl	.Lnegate_a
3:
#else
	eorlt	NEG, NEG, #3	/* flip quotient sign, flip remainder sign */
	bllt	.Lnegate_a
#endif

	/*
	 * Arguments are setup, allocate some stack for the remainder
	 * and call __qdivrem for the heavy lifting.
	 */
#ifdef __ARM_EABI__
	.cfi_def_cfa_offset 32
#endif
	sub	sp, sp, #16
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	adds	r4, sp, #8
#else
	mov	r4, sp
	adds	r4, r4, #8
#endif
	str	r4, [sp]
	bl	PLT_SYM(__qdivrem)
	add	sp, sp, #8
#ifdef __ARM_EABI__
	.cfi_def_cfa_offset 24
	.cfi_offset 3, -20
	.cfi_offset 2, -24
#endif

	/*
	 * The quotient is already in the right place and neither value
	 * needs its sign flipped.
	 */
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	NEG, .Lnegate_neither
#else
	cmp	NEG, #0		/* any signs to flip? */
	beq	.Lnegate_neither
#endif

	cmp	NEG, #2		/* does remainder need to be negative? */
	beq	.Lnegate_b_only	/* 2 means b only */
	bgt	.Lnegate_both	/* 3 means both */
.Lnegate_a_only:
	bl	.Lnegate_a	/* 1 means a only */
.Lnegate_neither:
	pop	{r2-r6, pc}	/* grab b from stack */
.Lnegate_both:
	bl	.Lnegate_a
.Lnegate_b_only:
	pop	{r2-r3}		/* get remainder */
#ifdef __ARM_EABI__
	.cfi_def_cfa_offset 16
#endif
	bl	.Lnegate_b	/* negate it */
	pop	{r4-r6, pc}

	.align	0
.Lnegate_a:
#ifdef __thumb__
	movs	r4, AHI
	movs	AHI, #0
	negs	ALO, ALO
	sbcs	AHI, AHI, r4
#else
	negs	ALO, ALO
	rsc	AHI, AHI, #0
#endif
	RET

	.align	0
.Lnegate_b:
#ifdef __thumb__
	movs	r4, BHI
	movs	BHI, #0
	negs	BLO, BLO
	sbcs	BHI, BHI, r4
#else
	negs	BLO, BLO
	rsc	BHI, BHI, #0
#endif
	RET

	.align	0
.Lmaxdenom:
	/*
	 * We had a carry so the denominator must have INT64_MIN
	 * Also BLO and BHI never changed values so we can use
	 * them to see if the numerator has the same value.  We
	 * don't have to worry about sign.
	 */
	cmp	BHI, AHI
#ifdef __thumb__
	bne	1f
	cmp	BLO, ALO
#else
	cmpeq	BLO, ALO
#endif
	bne	1f

	/*
	 * They were equal, so we return a quotient of 1 and remainder of 0.
	 */
	movs	ALO, #1
	movs	AHI, #0
	movs	BLO, #0
	movs	BHI, #0
	pop	{r4-r6, pc}

	/*
	 * Our remainder must be the numerator and our quotient is 0.
	 */
	.align	0
1:	movs	BLO, ALO
	movs	BHI, AHI
	movs	ALO, #0
	movs	AHI, #0
	pop	{r4-r6, pc}

#if !defined(_KERNEL) && !defined(_STANDALONE)
	.align	0
.Ldivbyzero:
	push	{r0-r1,r4,lr}	
#ifdef __ARM_EABI__
# if !defined(__ARM_DWARF_EH__)
	.save	{r0-r1,r4,lr}	
# endif
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -4
	.cfi_offset  4, -8
#endif
	cmp	AHI, #0
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
#ifdef __thumb__
	ittee	ge
#endif
	mvnge	ALO, #0
	mvnge	AHI, #0x80000000
	movlt	ALO, #0
	movlt	AHI, #0x80000000
#else
	blt	1f
	movs	ALO, #0
	mvns	ALO, ALO
	mov	AHI, ALO
	lsrs	AHI, AHI, #1
	b	2f
1:
	movs	ALO, #0
	movs	AHI, #1
	lsls	AHI, AHI, #31
2:
#endif /* __thumb__ && !_ARM_ARCH_T2 */
	bl	PLT_SYM(__aeabi_ldiv0)
	pop	{r2-r4, pc}
#endif	/* !_KERNEL && !_STANDALONE */
#ifdef __ARM_EABI__
	.cfi_endproc
# if !defined(__ARM_DWARF_EH__)
	.fnend
# endif
#endif
END(__aeabi_ldivmod)
