/* $NetBSD: memset.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

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

ENTRY(memset)
	cbz	x2, .Lret
	mov	x15, x0			/* working data pointer */
	cbz	x1, .Lzerofill
	cbz	x1, .Lfilled
	/*
	 * Non zero fill, replicate to all 64 bits of x1.
	 */
	and	x1, x1, #0xff
	orr	x1, x1, x1, lsl #8
	orr	x1, x1, x1, lsl #16
	orr	x1, x1, x1, lsl #32
.Lfilled:
	cmp	x2, #15			/* if it's small, ignore alignment */
	b.ls	.Llast_subqword

	mov	x6, x1
	tst	x15, #15
	b.eq	.Lqword_loop

/*
 * We have at least 15 to copy which means we can get qword alignment
 * without having to check the amount left.
 */
	tbz	x15, #0, .Lhword_aligned
	strb	w1, [x15], #1
.Lhword_aligned:
	tbz	x15, #1, .Lword_aligned
	strh	w1, [x15], #2
.Lword_aligned:
	tbz	x15, #2, .Ldword_aligned
	str	w1, [x15], #4
.Ldword_aligned:
	tbz	x15, #3, .Lqword_aligned
	str	x1, [x15], #8
/*
 * Now we qword aligned. Figure how much we have to write to get here.
 * Then subtract from the length.  If we get 0, we're done.
 */
.Lqword_aligned:
	sub	x5, x15, x0 
	subs	x2, x2, x5
	b.eq	.Lret

/*
 * Write 16 bytes at time.  If we don't have 16 bytes to write, bail.
 * Keep looping if there's data to set.
 */
.Lqword_loop:
	subs	x2, x2, #16
	b.mi	.Llast_subqword
	stp	x1, x6, [x15], #16
	b.ne	.Lqword_loop
	ret

/*
 * We have less than a qword to write.  We hope we are aligned but since
 * unaligned access works, we don't have to be aligned.
 */
.Llast_subqword:
	tbz	x2, #3, .Llast_subdword
	str	x1, [x15], #8
.Llast_subdword:
	tbz	x2, #2, .Llast_subword
	str	w1, [x15], #4
.Llast_subword:
	tbz	x2, #1, .Llast_subhword
	strh	w1, [x15], #2
.Llast_subhword:
	tbz	x2, #0, .Lret
	strb	w1, [x15]
.Lret:	ret

/*
 * If we are filling with zeros then let's see if we can use the
 *	dc zva, <Xt>
 * instruction to speed things up.
 */
.Lzerofill:
	mrs	x9, dczid_el0
	/*
	 * Make sure we can the instruction isn't prohibited.
	 */
	tbnz	x9, #4, .Lfilled
	/*
	 * Now find out the block size.
	 */
	ubfx	x9, x9, #0, #4	/* extract low 4 bits */
	add	x9, x9, #2	/* add log2(word) */
	mov	x10, #1		/* the value is log2(words) */
	lsl	x10, x10, x9	/* shift to get the block size */
	cmp	x2, x10		/* are we even copying a block? */
	b.lt	.Lfilled	/*   no, do it 16 bytes at a time */
	/*
	 * Now we figure out how many aligned blocks we have
	 */
	sub	x11, x10, #1	/* make block size a mask */
	add	x12, x15, x11	/* round start to a block boundary */
	asr	x12, x12, x9	/* "starting" block number */
	add	x13, x15, x2	/* get ending address */
	asr	x13, x13, x9	/* "ending" block numebr */
	cmp	x13, x12	/* how many blocks? */
	b.eq	.Lfilled	/*   none, do it 16 bytes at a time */

	/*
	 * Now we have one or more blocks to deal with.  First now we need
	 * to get block aligned.
	 */
	and	x7, x15, x11	/* are already aligned on a block boundary? */
	cbz	x7, .Lblock_aligned

	sub	x7, x10, x7	/* subtract offset from block length */
	sub	x2, x2, x7	/* subtract that from length */
	asr	x7, x7, #2	/* qword -> word */

	tbz	x15, #0, .Lzero_hword_aligned
	strb	wzr, [x15], #1
.Lzero_hword_aligned:
	tbz	x15, #1, .Lzero_word_aligned
	strh	wzr, [x15], #2
.Lzero_word_aligned:
	tbz	x15, #2, .Lzero_dword_aligned
	str	wzr, [x15], #4
.Lzero_dword_aligned:
	tbz	x15, #3, .Lzero_qword_aligned
	str	xzr, [x15], #8
.Lzero_qword_aligned:
	cbz	x7, .Lblock_aligned /* no qwords? just branch */
	adr	x6, .Lblock_aligned
	sub	x6, x6, x7	/* backup to write the last N qwords */
	br	x6		/* and do it */
	/*
	 * This is valid for cache lines <= 256 bytes.
	 */
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16
	stp	xzr, xzr, [x15], #16

/*
 * Now we are block aligned.
 */
.Lblock_aligned:
	subs	x2, x2, x10
	b.mi	.Lblock_done
	dc	zva, x15
	add	x15, x15, x10
	b.ne	.Lblock_aligned
	ret

.Lblock_done:
	and	x2, x2, x12	/* make positive again */
	mov	x6, xzr		/* fill 2nd xword */
	b	.Lqword_loop	/* and finish filling */

END(memset)
