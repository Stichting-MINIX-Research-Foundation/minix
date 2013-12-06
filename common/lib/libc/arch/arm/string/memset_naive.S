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

RCSID("$NetBSD: memset_naive.S,v 1.1 2013/01/08 20:15:00 matt Exp $")

/*
 * This isn't quite as simple/short as it could be but the truly trivial
 * memset was an order of magnitude slower than this.
 */

ENTRY(memset)
/* LINTSTUB: void *memset(void *, int, size_t) */
	mov	ip, r0			/* need to preserve r0 */
	cmp	r2, #10			/* 10 bytes or less? */
	ble	.Lbyte_by_byte		/*    yes, bytewise is faster */
	ands	r3, r1, #0xff		/* we are dealing with bytes */
	orrne	r3, r3, r3, lsl #8	/* move value into 2nd byte lane */
	orrne	r3, r3, r3, lsl #16	/* move value into all byte lanes */
	mov	r1, r2			/* move count */
	ands	r2, ip, #7		/* are we dword aligned? */
	beq	1f			/*   yes we are */
	rsb	r2, r2, #8		/* how many bytes until aligned? */
	sub	r1, r1, r2		/* subtract from count */
	tst	ip, #1			/* halfword aligned? */
	strneb	r3, [ip], #1		/*   nope, write a byte */
	tst	ip, #2			/* word aligned? */
	strneh	r3, [ip], #2		/*   nope, write a halfword */
	tst	ip, #4			/* dword aligned? */
	strne	r3, [ip], #4		/*   nope, write a word */
	/*
	 * At this point, we are dword aligned.
	 */
1:	mov	r2, r3			/* duplicate fill value */
2:	subs	r1, r1, #16		/* can we write 16 bytes? */
	stmgeia	ip!, {r2,r3}		/*   yes, write the first 8 of them */
	stmgeia	ip!, {r2,r3}		/*   yes, write the second 8 of them */
	bgt	2b			/* more left to fill */
	RETc(eq)			/*   no, return */
	/*
	 * Our count went negative but the bits below 16 haven't changed.
	 * So we are effectively testing modulo 16.
	 */
	tst	r1, #8			/* can we write at least 8 bytes? */
	stmneia	ip!, {r2,r3}		/*   so do it */
	tst	r1, #4			/* can we write at least 4 bytes? */
	strne	r3, [ip], #4		/*   so do it */
	tst	r1, #2			/* can we write at least 2 bytes? */
	strneh	r3, [ip], #2		/*   so do it */
	tst	r1, #1			/* can we write 1 bytes? */
	strneb	r3, [ip], #1		/*   so do it */
	RET				/* return */

.Lbyte_by_byte:
	subs	r2, r2, #1		/* can we write a byte? */
	RETc(lt)			/*   no, return */
	strb	r3, [ip], #1		/* write a byte */
	b	.Lbyte_by_byte		/* do next byte */
END(memset)
