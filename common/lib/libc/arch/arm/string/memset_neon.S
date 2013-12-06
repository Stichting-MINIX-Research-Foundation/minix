/*	$NetBSD: memset_neon.S,v 1.1 2012/12/13 01:41:59 matt Exp $	*/

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
#include "assym.h"

/*
 * memset: Sets a block of memory to the specified value
 * Using NEON instructions
 *
 * On entry:
 *   r0 - dest address
 *   r1 - byte to write
 *   r2 - number of bytes to write
 *
 * On exit:
 *   r0 - dest address
 */
/* LINTSTUB: Func: void *memset(void *, int, size_t) */
ENTRY(memset)
	and		r3, r1, #0xff	/* We deal with bytes */
	mov		r1, r2
	mov		ip, r0		/* r0 needs to stay the same */

	vdup.8		q0, r3		/* move fill to SIMD */
	/* we no longer need to keep the fill value in a ARM register */

	/* Ok first we will dword align the address */
	ands		r2, ip, #7	/* grab the bottom three bits */
	beq		.Lmemset_dwordaligned	/* The addr is dword aligned */

	bic		ip, ip, #7	/* clear bottom three bits of addr */
	vldr		d7, [ip]	/* load from memory */
	add		r1, r1, r2	/* add "pre-fill" to length */
	lsl		r2, r2, #3	/* byte to no-fill bit count */

#ifdef __ARMEB__
	neg		r1, r1		/* start from the MSB */
#endif
	vmov		s4, r2		/* move to SIMD d2 */
	vmvn.u64	d3, #0		/* set all ones */
	vshl.u64	d3, d3, d2	/* create a no-fill mask */
	vmvn.u64	d2, d3		/* invert mask for a fill-mask */
	vand		d7, d7, d3	/* mask out fill bits */
	vand		d2, d0, d2	/* mask out no-fill bits */
	vorr		d7, d2, d7	/* merge fill and memory */

	cmp		r1, #8		/* Do we have less than 8 bytes */
	movlt		r2, #0		/* indicate this is the last word */
	blt		.Lmemset_lessthaneight_noload

	vstmia		ip!, {d7}	/* write back to memory */
	subs		r1, r1, #8	/* and remove 8 bytes from the length */
	RETc(eq)

	/* We are now doubleword aligned */
.Lmemset_dwordaligned:
	vmov		q1, q0		/* put fill in q1 (d2-d3) */
	vmov		q2, q0		/* put fill in q2 (d4-d5) */
	vmov		q3, q0		/* put fill in q3 (d6-d7) */

	and		r2, ip, #63	/* check for 64-byte alignment */
	beq		.Lmemset_8dwordaligned
	/*
	 * Let's align to a 64-byte boundary so that stores don't cross
	 * cacheline boundaries.  We also know we have at least 128-bytes to
	 * copy so we don't have to worry about the length at the moment.
	 */
	rsb		r2, r2, #64	/* how many bytes until 64 bytes */
	cmp		r1, r2		/* compare against length */
	andlt		r2, r1, #0x38 	/* if < len, use trunc(len, 8) */
	subs		r1, r1, r2	/* subtract from len */
	add		pc, pc, r2	/* and jump to it */
	nop
	RETc(eq);			b	.Lmemset_lessthaneight
	vstmia		ip!, {d0};	b	.Lmemset_8dwordaligned
	vstmia		ip!, {d0-d1};	b	.Lmemset_8dwordaligned
	vstmia		ip!, {d0-d2};	b	.Lmemset_8dwordaligned
	vstmia		ip!, {d0-d3};	b	.Lmemset_8dwordaligned
	vstmia		ip!, {d0-d4};	b	.Lmemset_8dwordaligned
	vstmia		ip!, {d0-d5};	b	.Lmemset_8dwordaligned
	vstmia		ip!, {d0-d6}
.Lmemset_8dwordaligned:
	vmov		d0, d1		/* restore in case of unaligned start */
	cmp		r1, #8		/* do we have less than 8 bytes */
	movlt		r2, #0		/* indicate last word */
	blt		.Lmemset_lessthaneight

	cmp		r1, #512
	blt		.Lmemset_sub512

	/* Do 512 bytes at a time */
	mov		r2, #512
.Lmemset_512:
	vstmia		ip!, {d0-d7}
	vstmia		ip!, {d0-d7}
	vstmia		ip!, {d0-d7}
	vstmia		ip!, {d0-d7}
	vstmia		ip!, {d0-d7}
	vstmia		ip!, {d0-d7}
	vstmia		ip!, {d0-d7}
	vstmia		ip!, {d0-d7}
.Lmemset_0:
	subs		r1, r1, r2
	RETc(eq)			/* return if done */
	cmp		r1, #512
	bge		.Lmemset_512

	/*
	 * We have less than 512 bytes left, but since the sequence above
	 * store 64 bytes at a time, we determine the number of instructions
	 * we need to store the remainder (if >= 64 bytes) and execute that
	 * many vstmia.
	 */
.Lmemset_sub512:
	lsr		r2, r1, #6	/* divide by 64 */
	lslne		r4, r2, #2	/* multiply by 4 */
	addne		r4, r4, #1f + 8 - .Lmemset_0
					/* add the # of bytes between */
1:	subne		pc, r4 		/* and go */

	/*
	 * We have less than 64 bytes to copy on a 8dword aligned address
	 */
	and		r2, r1, #56	/* get # of full dwords */
	ands		r1, r1, #7	/* get # of extra bytes */
	beq		.Lmemset_finalstore
	/*
	 * The last word is a partial fill so load its value and update it
	 * to include the fill value.
	 */
.Lmemset_lessthaneight:
	vldr		d7, [ip, r2]	/* load the last partial dword */
.Lmemset_lessthaneight_noload:
	lsl		r1, r1, #3	/* byte to fill bit count */
#ifdef __ARMEB__
	neg		r1, r1		/* start from the MSB */
#endif
	vmov		s4, r1		/* move to SIMD d2 */
	vmvn.u64	d3, #0		/* set all ones */
	vshl.u64	d3, d3, d2	/* create a no-fill mask */
	vmvn.u64	d2, d3		/* invert mask */
	vand		d7, d7, d2	/* keep no-fill bits */
	vand		d2, d0, d3	/* mask out no-fill bits */
	vorr		d7, d2, d7	/* merge fill and no-fill */
	vmov		q1, q0		/* restore d2 & d3 */
	add		r2, r2, #8	/* compensate for the partial dword */
.Lmemset_finalstore:
	add		pc, pc, r2	/* and jump to it */
	nop
	vstr		d7, [ip];	RET
	vstmia		ip, {d6-d7};	RET
	vstmia		ip, {d5-d7};	RET
	vstmia		ip, {d4-d7};	RET
	vstmia		ip, {d3-d7};	RET
	vstmia		ip, {d2-d7};	RET
	vstmia		ip, {d1-d7};	RET
	vstmia		ip, {d0-d7};	RET
END(memset)
