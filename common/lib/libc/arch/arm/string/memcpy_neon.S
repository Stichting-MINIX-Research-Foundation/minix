/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
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

RCSID("$NetBSD: memcpy_neon.S,v 1.1 2013/01/03 09:34:44 matt Exp $")

	.text
ENTRY(memcpy)
	teq	r2, #0			/* 0 length? */
	cmpne	r0, r1			/*   if not, does src == dst? */
	RETc(eq)			/*   yes, (to either) return */

	mov	r3, r0			/* keep r0 unchanged */
#if 0
	cmp	r2, #16			/* copy less than 8 bytes? */
	bge	.Ldst_aligner		/*   nope, do it the long way */

1:	ldrb	ip, [r1], #1		/* load a byte from src */
	subs	r2, r2, #1		/* and more to transfer? */
	strb	ip, [r3], #1		/* save it to dst */
	bne	1b			/*   yes, do next byte */
	RET				/* return */
#endif

.Ldst_aligner:
	tst	r3, #7			/* is dst pointer word aligned? */
	beq	.Lsrc_aligner		/*   yes, check src pointer */
	/*
	 * Until the dst pointer is word aligned, read src and dst byte by
	 * byte until it is aligned or we've copied everything.
	 */
	ldrb	ip, [r1], #1		/* load a byte from src */
	strb	ip, [r3], #1		/* save the byte to dst */
	subs	r2, r2, #1		/* end of transfer? */
	bne	.Ldst_aligner		/*   no, try next byte */
	RET				/* yes, we're done! */

.Lsrc_aligner:
	push	{r4-r5}			/* save some registers */
	add	r4, r2, r3		/* keep a pointer to the end of src */
	ands	r5, r1, #7		/* get misalignment of src pointer */
	beq	.Lcongruent_main	/*   aligned, do it the fast way */

	vdup.8	d1, r5			/* set offset for table */
	rsb	r5, r5, #8		/* calculate leftover of each word */
	bic	r1, r1, #7		/* dword align src pointer */

	vldr	d0, .Ltbl_value		/* load table value */
	vadd.u8	d0, d0, d1		/* add offset to it */

	vld1.64 {d1}, [r1:64]!		/* load a dword from src */

	cmp	r2, r5			/* do we already have enough? */
	bgt	.Lincongruent		/*   no, so read more */

.Lincongruent_finish:
	vtbl.8	d0, {d1-d2}, d0		/* merge last dwords */
	cmp	r2, #8			/* room for a full dword? */ 
#ifdef __ARMEB__
	vrev64.32 d0, d0		/* word swap to LE */
#endif
	blt	.Lfinish		/*   no, write final partial dword */
	vst1.32 {d0}, [r3:64]		/*   yes, write final full dword */
	b	.Ldone			/* and we're done! */

.Lincongruent:
	vld1.64 {d2}, [r1:64]!		/* load a dword */
	cmp	r2, #8			/* can we write a full dword? */
	blt	.Lincongruent_finish	/*   no, finish it. */
	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vst1.64 {d1}, [r3:64]!		/* store a dword */
	subs	r2, r2, #8		/* have we written everything? */
	beq	.Ldone			/*   yes, we're done! */
	vmov	d1, d2			/* prepare for next dword */
	tst	r3, #63			/* are we 64-byte aligned? */
	bne	.Lincongruent		/*   no, load next dword */

	/*
	 * We are now 64-byte aligneds so all writes should fill one or more
	 * cachelines.  Even if d1 has 7 bytes cached, to write 32 bytes we
	 * still need to read 4 dwords (3 full dwords and 1 dword for that
	 * last byte).
	 */
	cmp	r2, #32			/* can we write 4 more dwords? */
	blt	.Lincongruent_dword	/*   no, handle dword by dword */
	vld1.64 {d2-d5}, [r1:64]!	/* read 4 dwords */
	cmp	r2, #64			/* can we write 4 more dwords? */
	blt	.Lincongruent_4dword	/*   no, handle it */

1:	vld1.64 {d7-d10}, [r1:64]!	/* read 4 dwords */
	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vtbl.8	d2, {d2-d3}, d0		/* reorder */
	vtbl.8	d3, {d3-d4}, d0		/* reorder */
	vtbl.8	d4, {d4-d5}, d0		/* reorder */
	vst1.64 {d1-d4}, [r3:64]!	/* write 4 dwords */
	vmov	d6, d5			/* move out of the way the load */
	cmp	r2, #96			/* have 8+4 dwords to write? */
	blt	2f			/*   no more data, skip the load */
	vld1.64 {d2-d5}, [r1:64]!	/* more data, load 4 dwords */
2:	vtbl.8	d6, {d6-d7}, d0		/* reorder */
	vtbl.8	d7, {d7-d8}, d0		/* reorder */
	vtbl.8	d8, {d8-d9}, d0		/* reorder */
	vtbl.8	d9, {d9-d10}, d0	/* reorder */
	vst1.64 {d6-d9}, [r3:64]!	/* write 4 dwords */
	subs	r2, r2, #64
	beq	.Ldone
	vmov	d1, d10
	cmp	r2, #64
	bge	1b

	/*
	 * we have leftovers in d1 and new untranslated date in d2-d5.
	 */
.Lincongruent_4dword:
	cmp	r2, #32
	blt	.Lincongruent_dword

	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vtbl.8	d2, {d2-d3}, d0		/* reorder */
	vtbl.8	d3, {d3-d4}, d0		/* reorder */
	vtbl.8	d4, {d4-d5}, d0		/* reorder */
	vst1.64 {d1-d4}, [r3:64]!	/* write 4 dwords */
	vmov	d1, d5			/* move leftovers */
	subs	r2, r2, #32
	beq	.Ldone

.Lincongruent_dword:
#if 0
	cmp	r2, r5			/* enough in leftovers? */
	ble	.Lincongruent_finish	/*   yes, finish it. */
	vld1.64 {d2}, [r1:64]!		/* load a dword */
	cmp	r2, #8			/* can we write a full dword? */
	blt	.Lincongruent_finish	/*   no, finish it. */
	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vst1.64 {d1}, [r3:64]!		/* store a dword */
	subs	r2, r2, #8		/* have we written everything? */
	beq	.Ldone			/*   yes, we're done! */
	b	.Lincongruent_dword	/* and go get it */
#else
	cmp	r2, r5			/* are the bytes we have enough? */
	ble	.Lincongruent_finish	/*   yes, finish it. */
	mov	ip, r2			/* get remaining count */
	bic	ip, ip, #7		/* truncate to a dword */
	rsb	ip, ip, #32		/* subtract from 32 */
	ands	r2, r2, #7		/* count mod 8 */
	add	pc, pc, ip, lsl #1	/* and jump! */
	nop
	vld1.64 {d2}, [r1:64]!		/* load a dword */
	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vst1.64 {d1}, [r3:64]!		/* store a dword */
	vmov	d1, d2			/* prepare for next dword */
	vld1.64 {d2}, [r1:64]!		/* load a dword */
	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vst1.64 {d1}, [r3:64]!		/* store a dword */
	vmov	d1, d2			/* prepare for next dword */
	vld1.64 {d2}, [r1:64]!		/* load a dword */
	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vst1.64 {d1}, [r3:64]!		/* store a dword */
	vmov	d1, d2			/* prepare for next dword */
	vld1.64 {d2}, [r1:64]!		/* load a dword */
	vtbl.8	d1, {d1-d2}, d0		/* reorder */
	vst1.64 {d1}, [r3:64]!		/* store a dword */
	vmov	d1, d2			/* prepare for next dword */
	beq	.Ldone
	vld1.64 {d2}, [r1:64]!		/* load a dword */
	b	.Lincongruent_finish	/* write last partial dowrd */
#endif

.Lcongruent_main:
	vld1.32 {d0}, [r1:64]!		/* load next dword */
	cmp	r2, #8			/* compare current ptr against end */
	blt	.Lfinish		/*   greater so write final dword */
	vst1.32 {d0}, [r3:64]!		/* store dword */
	subs	r2, r2, #8		/* compare current ptr against end */
	beq	.Ldone			/*   equal? we're done! */
	tst	r3, #63			/* have we hit a 64-byte boundary? */
	bne	.Lcongruent_main	/*   no, write next word */

	cmp	r2, #64			/* can we write 4 dwords? */
	blt	.Lcongruent_loop	/*   no, this dword by dword */
	vldm	r1!, {d0-d7}		/* load next 7 dwords */
	cmp	r2, #128		/* can we write 16 dwords */
	blt	3f			/*   no, then deal with 8 dwords */

	/*
	 * The following writes two 64-byte interleaving stores and loads.
	 */
1:	vldm	r1!, {d8-d15}		/* load next 8 dwords */
	vstm	r3!, {d0-d7}		/* store 8 more dwords */
	cmp	r2, #192		/* can we write 16+8 dwords? */
	blt	2f			/*   no, don't load the next 8 dwords */
	vldm	r1!, {d0-d7}		/*   yes, load next 8 dwords */
2:	vstm	r3!, {d8-d15}		/* store 8 more dwords */
	sub	r2, r2, #128		/* we just stored 16 (8+8) dwords */
	beq	.Ldone			/*   if 0, we're done! */
	cmp	r2, #128		/* can we write 16 dwords */
	bge	1b			/*   yes, do it again */
	cmp	r2, #64			/* have we loaded 8 dwords? */
	blt	.Lcongruent_loop	/*   no, proceed to do it dword */

	/*
	 * We now have 8 dwords we can write in d0-d7.
	 */
3:	vstm	r3!, {d0-d7}		/* store 8 more dwords */
	subs	r2, r2, #64		/* we wrote 8 dwords */
	beq	.Ldone			/*   if 0, we're done! */

.Lcongruent_loop:
	vld1.32 {d0}, [r1]!		/* load dword from src */
	cmp	r2, #8			/* can we write a full dword? */
	blt	.Lfinish		/*   no, write last partial dword */
.Lcongruent_loop_start:
	vst1.32 {d0}, [r3]!		/* store dword into dst */
	subs	r2, r2, #8		/* subtract it from length */
	beq	.Ldone			/*   if 0, we're done! */
	vld1.32 {d0}, [r1]!		/* load dword from src */
	cmp	r2, #8			/* can we write a full dword? */
	bge	.Lcongruent_loop_start	/*   yes, so do it */

.Lfinish:
	vmov	r4, r5, d0		/* get last dword from NEON */
	tst	r2, #4			/* do we have at least 4 bytes left? */
	strne	r4, [r3], #4		/* store the 1st word */
	movne	r4, r5			/* move 2nd word into place */
	tst	r2, #2			/* do we have at least 2 bytes left? */
#ifdef __ARMEB__
	movne	r4, r4, ror #16		/*   yes, swap halfwords */
#endif
	strneh	r4, [r3], #2		/*   yes, store the halfword */
#ifdef __ARMEL__
	movne	r4, r4, lsr #16		/*   yes, discard just written bytes */
#endif
	tst	r2, #1			/* do we have a final byte? */
#ifdef __ARMEB__
	movne	r4, r4, lsr #24		/*   yes, move MSB to LSB */
#endif
	strneb	r4, [r3], #1		/*   yes, store it */

.Ldone:
	pop	{r4-r5}			/* restore registers */
	RET

	.p2align 3
.Ltbl_value:
#ifdef __ARMEL__
	.quad	0x0706050403020100
#else
	.quad	0x0001020304050607
#endif
END(memcpy)
