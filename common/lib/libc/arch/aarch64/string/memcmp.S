/* $NetBSD: memcmp.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
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

RCSID("$NetBSD: memcmp.S,v 1.1 2014/08/10 05:47:35 matt Exp $")

ENTRY(memcmp)
	mov	x9, x0
	mov	x10, x1
	mov	x0, xzr
	cbz	x2, .Lmemcmp_ret
#ifdef _KERNEL
	cmp	x2, #6
	b.eq	.Lmemcmp_6bytes
#endif
	cmp	x2, #7
	b.ls	.Lmemcmp_lessthan8

	ands	x3, x9, #7
	b.eq	.Lmemcmp_dword_loop

/*
 * The two addresses have identical alignment but are not yet dword aligned.
 */
	add	x2, x2, x3		/* add unalignment to length */
	sub	x2, x2, #8		/* now subtract a dword */

	sub	x9, x9, x3		/* dword align src1 */
	sub	x10, x10, x3		/* adjust src2 */

	lsl	x3, x3, #3		/* convert bytes to bits */
	ldr	x4, [x9], #8		/* load dword from src1 */
	ldr	x6, [x10], #8		/* load dword from src2 */
#ifdef __AARCH64EB__
	lsl	x4, x4, x3		/* discard leading bytes from data1 */
	lsl	x6, x6, x3		/* discard leading bytes from data2 */
#else
	lsr	x4, x4, x3		/* discard leading bytes from data1 */
	lsr	x6, x6, x3		/* discard leading bytes from data2 */
#endif
	subs	x0, x4, x6		/* compare data */
#ifdef __AARCH64EL__
	b.ne	.Lmemcmp_last_compare	/* difference.  find it */
#else
	b.eq	.Lmemcmp_dword_loop	/* no difference.  go to loop */
	rev	x4, x4			/* byte swap data1 */
	rev	x6, x6			/* byte swap data2 */
	b	.Lmemcmp_last_compare	/* go find the difference. */
#endif

.Lmemcmp_dword_loop:
	subs	x2, x2, #8
	b.mi	.Lmemcmp_finish_dword
	ldr	x4, [x9], #8
	ldr	x6, [x10], #8
	subs	x0, x4, x6
	b.eq	.Lmemcmp_dword_loop	/* no difference.  go to loop */
#ifdef __AARCH64EB__
	rev	x4, x4			/* byte swap data1 */
	rev	x6, x6			/* byte swap data2 */
#endif
	b	.Lmemcmp_last_compare	/* go find the difference. */

.Lmemcmp_finish_dword:
	/*
	 * we might have gotten here with nothing left.  If so, just bail.
	 */
	tst	x2, #7
	b.eq	.Lmemcmp_ret
	/*
	 *
	 */
	tbz	x2, #2, .Lmemcmp_finish_word
	ldr	w4, [x9], #4
	ldr	w6, [x10], #4
#ifdef __AARCH64EB__
	lsl	x4, x4, #32		/* move to MSW */
	lsl	x6, x6, #32		/* move to MSW */
#endif

.Lmemcmp_finish_word:
	tbz	x2, #1, .Lmemcmp_finish_hword
	ldrh	w5, [x9], #2
	ldrh	w7, [x10], #2
#ifdef __AARCH64EB__
	orr	x4, x4, x5, lsl #16
	orr	x6, x6, x7, lsl #16
#else
	orr	x4, x4, x5, lsl #32
	orr	x6, x6, x7, lsl #32
#endif

.Lmemcmp_finish_hword:
#ifdef __AARCH64EB__
	rev	x4, x4			/* byte swap data1 */
	rev	x6, x6			/* byte swap data1 */
#endif
	tbz	x2, #0, .Lmemcmp_last_compare
	ldrb	w5, [x9]
	ldrb	w7, [x10]
	orr	x4, x4, x5, lsl #48
	orr	x6, x6, x7, lsl #48
	b	.Lmemcmp_last_compare	/* go find the difference. */

/*
 * D
 */
.Lmemcmp_lessthan8:
	sub	x2, x2, #1
1:	ldrb	w4, [x9], #1
	ldrb	w5, [x10], #1
	subs	x2, x2, #1
	ccmp	x4, x5, #0, cs
	b.eq	1b
	sub	x0, x4, x5

.Lmemcmp_ret:
	ret

#ifdef _KERNEL
.Lmemcmp_6bytes:
	ldr	w4, [x9], #4
	ldrh	w5, [x9]
#if __AARCH64EB__
	orr	x4, x4, x5, lsl #48
	rev	x4, x4
#else
	orr	x4, x4, x5, lsl #32
#endif
	ldr	w6, [x10], #4
	ldrh	w7, [x10]
#if __AARCH64EB__
	orr	x6, x6, x7, lsl #48
	rev	x6, x6
#else
	orr	x6, x6, x7, lsl #32
#endif
#endif /* _KERNEL */

/*
 * We have loaded the final bytes in x4 and x6 in LE format.  Now we have
 * to figure what the difference is (if any).  First we subtract.  Any bytes
 * that are the same will be 0. So to find the first non-zero byte we byterev
 * and then use clz to find that byte.
 * We mask the location to get the start of the byte.  We shift both
 * data dwords left to remove the equal part.  Then we shift right to discard
 * the trailing bytes.  Then we subtract and return.
 */
	subs	x0, x4, x6
	b.eq	.Lmemcmp_ret
.Lmemcmp_last_compare:
	rev	x1, x0		/* byte reverse */
	clz	x1, x1		/* find first non-zero byte */
	bfi	x1, xzr, #0, #3	/* make it byte aligned */
	lsr	x0, x0, x1	/* shift to LSB */
	sxtb	w0, w0		/* sign extend */
	ret
END(memcmp)
