/*	$NetBSD: memcpy_arm.S,v 1.4 2013/08/11 04:56:32 matt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Neil A. Carson and Mark Brinicombe
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

#if !defined(__minix)
#if defined(__ARM_EABI__)
STRONG_ALIAS(__aeabi_memcpy, memcpy)
#endif
#endif /* !defined(__minix) */

/*
 * This is one fun bit of code ...
 * Some easy listening music is suggested while trying to understand this
 * code e.g. Iron Maiden
 *
 * For anyone attempting to understand it :
 *
 * The core code is implemented here with simple stubs for memcpy().
 *
 * All local labels are prefixed with Lmemcpy_
 * Following the prefix a label starting f is used in the forward copy code
 * while a label using b is used in the backwards copy code
 * The source and destination addresses determine whether a forward or
 * backward copy is performed.
 * Separate bits of code are used to deal with the following situations
 * for both the forward and backwards copy.
 * unaligned source address
 * unaligned destination address
 * Separate copy routines are used to produce an optimised result for each
 * of these cases.
 * The copy code will use LDM/STM instructions to copy up to 32 bytes at
 * a time where possible.
 *
 * Note: r12 (aka ip) can be trashed during the function along with
 * r0-r3 although r0-r2 have defined uses i.e. src, dest, len through out.
 * Additional registers are preserved prior to use i.e. r4, r5 & lr
 *
 * Apologies for the state of the comments ;-)
 */

/* For MINIX, we always spill r0, r4, r5, and lr, so we can easily
 * clean up the stack after a phys_copy fault. NetBSD, in contrast,
 * spills the minimum number of registers for each path.
 */
#if defined(__minix)
/* LINTSTUB: Func: void *phys_copy(void *src, void *dst, size_t len) */
ENTRY(phys_copy)
	/* switch the source and destination registers */
	eor     r0, r1, r0
	eor     r1, r0, r1
	eor     r0, r1, r0
#else
/* LINTSTUB: Func: void *memcpy(void *dst, const void *src, size_t len) */
ENTRY(memcpy)
#endif
	/* save leaf functions having to store this away */
#if defined(__minix)
	push	{r0, r4, r5, lr}	/* memcpy() returns dest addr */
#else
	push	{r0, lr}		/* memcpy() returns dest addr */
#endif

	subs	r2, r2, #4
	blt	.Lmemcpy_l4		/* less than 4 bytes */
	ands	r12, r0, #3
	bne	.Lmemcpy_destul		/* oh unaligned destination addr */
	ands	r12, r1, #3
	bne	.Lmemcpy_srcul		/* oh unaligned source addr */

.Lmemcpy_t8:
	/* We have aligned source and destination */
	subs	r2, r2, #8
	blt	.Lmemcpy_l12		/* less than 12 bytes (4 from above) */
	subs	r2, r2, #0x14
	blt	.Lmemcpy_l32		/* less than 32 bytes (12 from above) */
#if !defined(__minix)
	push	{r4}		/* borrow r4 */
#endif

	/* blat 32 bytes at a time */
	/* XXX for really big copies perhaps we should use more registers */
.Lmemcpy_loop32:
	ldmia	r1!, {r3, r4, r12, lr}
	stmia	r0!, {r3, r4, r12, lr}
	ldmia	r1!, {r3, r4, r12, lr}
	stmia	r0!, {r3, r4, r12, lr}
	subs	r2, r2, #0x20
	bge	.Lmemcpy_loop32

	cmn	r2, #0x10
	ldmiage	r1!, {r3, r4, r12, lr}	/* blat a remaining 16 bytes */
	stmiage	r0!, {r3, r4, r12, lr}
	subge	r2, r2, #0x10
#if !defined(__minix)
	pop	{r4}		/* return r4 */
#endif

.Lmemcpy_l32:
	adds	r2, r2, #0x14

	/* blat 12 bytes at a time */
.Lmemcpy_loop12:
	ldmiage	r1!, {r3, r12, lr}
	stmiage	r0!, {r3, r12, lr}
	subsge	r2, r2, #0x0c
	bge	.Lmemcpy_loop12

.Lmemcpy_l12:
	adds	r2, r2, #8
	blt	.Lmemcpy_l4

	subs	r2, r2, #4
	ldrlt	r3, [r1], #4
	strlt	r3, [r0], #4
	ldmiage	r1!, {r3, r12}
	stmiage	r0!, {r3, r12}
	subge	r2, r2, #4

.Lmemcpy_l4:
	/* less than 4 bytes to go */
	adds	r2, r2, #4
#if defined(__minix)
	popeq	{r0, r4, r5}
	moveq	r0, #0
	popeq	{pc}
#else
#ifdef __APCS_26_
	ldmiaeq sp!, {r0, pc}^		/* done */
#else
	popeq	{r0, pc}		/* done */
#endif
#endif
	/* copy the crud byte at a time */
	cmp	r2, #2
	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	ldrbge	r3, [r1], #1
	strbge	r3, [r0], #1
	ldrbgt	r3, [r1], #1
	strbgt	r3, [r0], #1
#if defined(__minix)
	pop	{r0, r4, r5}
	mov	r0, #0
	pop	{pc}
#else
	pop	{r0, pc}
#endif

	/* erg - unaligned destination */
.Lmemcpy_destul:
	rsb	r12, r12, #4
	cmp	r12, #2

	/* align destination with byte copies */
	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	ldrbge	r3, [r1], #1
	strbge	r3, [r0], #1
	ldrbgt	r3, [r1], #1
	strbgt	r3, [r0], #1
	subs	r2, r2, r12
	blt	.Lmemcpy_l4		/* less the 4 bytes */

	ands	r12, r1, #3
	beq	.Lmemcpy_t8		/* we have an aligned source */

	/* erg - unaligned source */
	/* This is where it gets nasty ... */
.Lmemcpy_srcul:
	bic	r1, r1, #3
	ldr	lr, [r1], #4
	cmp	r12, #2
	bgt	.Lmemcpy_srcul3
	beq	.Lmemcpy_srcul2
	cmp	r2, #0x0c
	blt	.Lmemcpy_srcul1loop4
	sub	r2, r2, #0x0c
#if !defined(__minix)
	push	{r4, r5}
#endif

.Lmemcpy_srcul1loop16:
#ifdef __ARMEB__
	mov	r3, lr, lsl #8
#else
	mov	r3, lr, lsr #8
#endif
	ldmia	r1!, {r4, r5, r12, lr}
#ifdef __ARMEB__
	orr	r3, r3, r4, lsr #24
	mov	r4, r4, lsl #8
	orr	r4, r4, r5, lsr #24
	mov	r5, r5, lsl #8
	orr	r5, r5, r12, lsr #24
	mov	r12, r12, lsl #8
	orr	r12, r12, lr, lsr #24
#else
	orr	r3, r3, r4, lsl #24
	mov	r4, r4, lsr #8
	orr	r4, r4, r5, lsl #24
	mov	r5, r5, lsr #8
	orr	r5, r5, r12, lsl #24
	mov	r12, r12, lsr #8
	orr	r12, r12, lr, lsl #24
#endif
	stmia	r0!, {r3-r5, r12}
	subs	r2, r2, #0x10
	bge	.Lmemcpy_srcul1loop16
#if !defined(__minix)
	pop	{r4, r5}
#endif
	adds	r2, r2, #0x0c
	blt	.Lmemcpy_srcul1l4

.Lmemcpy_srcul1loop4:
#ifdef __ARMEB__
	mov	r12, lr, lsl #8
#else
	mov	r12, lr, lsr #8
#endif
	ldr	lr, [r1], #4
#ifdef __ARMEB__
	orr	r12, r12, lr, lsr #24
#else
	orr	r12, r12, lr, lsl #24
#endif
	str	r12, [r0], #4
	subs	r2, r2, #4
	bge	.Lmemcpy_srcul1loop4

.Lmemcpy_srcul1l4:
	sub	r1, r1, #3
	b	.Lmemcpy_l4

.Lmemcpy_srcul2:
	cmp	r2, #0x0c
	blt	.Lmemcpy_srcul2loop4
	sub	r2, r2, #0x0c
#if !defined(__minix)
	push	{r4, r5}
#endif

.Lmemcpy_srcul2loop16:
#ifdef __ARMEB__
	mov	r3, lr, lsl #16
#else
	mov	r3, lr, lsr #16
#endif
	ldmia	r1!, {r4, r5, r12, lr}
#ifdef __ARMEB__
	orr	r3, r3, r4, lsr #16
	mov	r4, r4, lsl #16
	orr	r4, r4, r5, lsr #16
	mov	r5, r5, lsl #16
	orr	r5, r5, r12, lsr #16
	mov	r12, r12, lsl #16
	orr	r12, r12, lr, lsr #16
#else
	orr	r3, r3, r4, lsl #16
	mov	r4, r4, lsr #16
	orr	r4, r4, r5, lsl #16
	mov	r5, r5, lsr #16
	orr	r5, r5, r12, lsl #16
	mov	r12, r12, lsr #16
	orr	r12, r12, lr, lsl #16
#endif
	stmia	r0!, {r3-r5, r12}
	subs	r2, r2, #0x10
	bge	.Lmemcpy_srcul2loop16
#if !defined(__minix)
	pop	{r4, r5}
#endif
	adds	r2, r2, #0x0c
	blt	.Lmemcpy_srcul2l4

.Lmemcpy_srcul2loop4:
#ifdef __ARMEB__
	mov	r12, lr, lsl #16
#else
	mov	r12, lr, lsr #16
#endif
	ldr	lr, [r1], #4
#ifdef __ARMEB__
	orr	r12, r12, lr, lsr #16
#else
	orr	r12, r12, lr, lsl #16
#endif
	str	r12, [r0], #4
	subs	r2, r2, #4
	bge	.Lmemcpy_srcul2loop4

.Lmemcpy_srcul2l4:
	sub	r1, r1, #2
	b	.Lmemcpy_l4

.Lmemcpy_srcul3:
	cmp	r2, #0x0c
	blt	.Lmemcpy_srcul3loop4
	sub	r2, r2, #0x0c
#if !defined(__minix)
	push	{r4, r5}
#endif

.Lmemcpy_srcul3loop16:
#ifdef __ARMEB__
	mov	r3, lr, lsl #24
#else
	mov	r3, lr, lsr #24
#endif
	ldmia	r1!, {r4, r5, r12, lr}
#ifdef __ARMEB__
	orr	r3, r3, r4, lsr #8
	mov	r4, r4, lsl #24
	orr	r4, r4, r5, lsr #8
	mov	r5, r5, lsl #24
	orr	r5, r5, r12, lsr #8
	mov	r12, r12, lsl #24
	orr	r12, r12, lr, lsr #8
#else
	orr	r3, r3, r4, lsl #8
	mov	r4, r4, lsr #24
	orr	r4, r4, r5, lsl #8
	mov	r5, r5, lsr #24
	orr	r5, r5, r12, lsl #8
	mov	r12, r12, lsr #24
	orr	r12, r12, lr, lsl #8
#endif
	stmia	r0!, {r3-r5, r12}
	subs	r2, r2, #0x10
	bge	.Lmemcpy_srcul3loop16
#if !defined(__minix)
	pop	{r4, r5}
#endif
	adds	r2, r2, #0x0c
	blt	.Lmemcpy_srcul3l4

.Lmemcpy_srcul3loop4:
#ifdef __ARMEB__
	mov	r12, lr, lsl #24
#else
	mov	r12, lr, lsr #24
#endif
	ldr	lr, [r1], #4
#ifdef __ARMEB__
	orr	r12, r12, lr, lsr #8
#else
	orr	r12, r12, lr, lsl #8
#endif
	str	r12, [r0], #4
	subs	r2, r2, #4
	bge	.Lmemcpy_srcul3loop4

.Lmemcpy_srcul3l4:
	sub	r1, r1, #1
	b	.Lmemcpy_l4

#if defined(__minix)
LABEL(phys_copy_fault)		/* kernel can send us here */
	pop	{r0, r4, r5}
	pop	{pc}

LABEL(phys_copy_fault_in_kernel)	/* kernel can send us here */
	pop	 {r0, r4, r5}
	mrc	p15, 0, r0, c6, c0, 0	/* Read DFAR */
	pop	{pc}
#else
END(memcpy)
#endif
