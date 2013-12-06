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

RCSID("$NetBSD: strlen_neon.S,v 1.3 2012/12/28 05:15:08 matt Exp $")
	.text

ENTRY(strlen)
	mov	ip, r0		/* we use r0 for return value */
	ands	r1, r0, #15	/* verify qword alignment */
	neg	r0, r1		/* subtract misalignment from count */
	veor	q2, q2, q2	/* clear mask */
	mov	r3, #7		/* NBBY - 1 */
	vdup.32	q3, r3		/* dup throughout q3 */
	movw	r3, #0x0404	/* magic since there are 4 bytes per U32 */
	orr	r3, r3, lsl #16	/* copy to upper 16 bits */
	beq	.Lmain_loop
	veor	q0, q0, q0	/* clear q0 */
	vmvn	q2, q2		/* set all 16 bytes of mask to all 1s */
	bic	ip, ip, #15	/* qword align string address */
	lsl	r1, r1, #3	/* convert to bits */
	cmp	r1, #64
	rsbgt	r1, r1, #128	/*   > 64? BE so we are shifting LSW right */
	movgt	r2, #0		/*   > 64? leave MSW alone */
	rsble	r2, r1, #64	/*   <=64? BE so we are shifting MSW right */
	movle	r1, #64		/*   <=64? clear LSW */
	vmov	d0, r1, r2	/* set shifts for lower and upper halves */
	vmovl.u32 q0, d0	/* 2 U32 -> 2 U64 */
	vshl.u64 q2, q2, q0	/* shift */
	/*
	 * Main loop.  Load 16 bytes, do a clz, 
	 */
.Lmain_loop:
	vld1.64 {d0, d1}, [ip:128]!	/* load qword */
#ifdef __ARMEL__
	vrev64.8 q0, q0		/* convert to BE for clz */
#endif
	vswp	d0, d1		/* swap dwords to get BE qword */
	vorr	q0, q0, q2	/* or "in" leading byte mask */
	veor	q2, q2, q2	/* clear leading byte mask */
	vceq.i8	q1, q0, #0	/* test each byte for 0 */
	/* Why couldn't there be a 64-bit CLZ? */
	vclz.u32 q1, q1		/* count leading zeroes to find the 0 byte */
	vadd.u32 q1, q1, q3	/* round up to byte bounary */
	vshr.u32 q1, q1, #3	/* convert to bytes */
	vmovn.u32 d0, q1	/* 4 I32 -> 4 I16 */
	vmovn.u16 d0, q0	/* 4 I16 -> 4  I8 */
	vmov	r2, s0		/* get counts */
	eors	r2, r2, r3	/* xor with 0x04040404 */
	addeq	r0, #16		/*   0?  no NULs */
	beq	.Lmain_loop	/* get next qword */
	clz	ip, r2		/* count leading zeros */
	mov	r2, r2, lsl ip	/* discard them */
	mov	ip, ip, lsr #3	/* divide leading zeroes by 8 */
	add	r0, r0, ip, lsl #2 /* multiple by 4 and add to count */
	and	r2, r2, #(3 << 29)
	add	r0, r0, r2, lsr #29
	RET			/* and return. */
END(strlen)
