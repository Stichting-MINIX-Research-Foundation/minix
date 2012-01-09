/*	$NetBSD: memset.S,v 1.7 2011/01/17 07:07:36 matt Exp $ */

/*-
 * Copyright (C) 2003	Matt Thomas <matt@3am-software.com>
 * Copyright (C) 2001	Martin J. Laubach <mjl@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*----------------------------------------------------------------------*/

#include <machine/asm.h>

/*----------------------------------------------------------------------*/
/*
     void bzero(void *b r3, size_t len r4);
     void *memset(void *b r3, int c r4, size_t len r5);
*/
/*----------------------------------------------------------------------*/

#define r_dst	%r4
#define r_len	%r5
#define r_tmp	%r6
#define r_val	%r0

ENTRY(bzero)
		li	r_val, 0		/* Value to fill with */
		mr	r_len, %r4
		b	cb_memset

ENTRY(memset)
		mr	r_val, %r4		/* Value to fill with */

cb_memset:
		cmplwi	r_len, 0		/* is the length 0? */
		beqlr-				/*    nothing to do */
		mr	r_dst, %r3
		/*
		 * r3=start, r4=dstptr, r5=length, r0=fill-val
		 */
		cmplwi	r_len, 6		/* more than 6 bytes? */
		bgt	complex_fill		/*    do it the complex way */
		subi	r_dst, r_dst, 1		/* presubtract for stbu */
simple_fill:	mtctr	r_len			/* set CTR */
1:		stbu	r_val, 1(r_dst)		/* update memory */
		bdnz+	1b			/*    until CTR is 0 */
		blr				/* return */

complex_fill:
		rlwimi	r_val, r_val, 8, 16, 23	/* word extend fill value */
		rlwimi	r_val, r_val, 16, 0, 15
		andi.	r_tmp, r_dst, 0x03
		beq+	word_fill		/* already aligned to word? */
		andi.	r_tmp, r_dst, 0x01
		beq	half_fill		/* aligned to halfword? */
		stb	r_val, 0(r_dst)
		addi	r_dst, r_dst, 1
		subi	r_len, r_len, 1		/* subtract byte */
		andi.	r_tmp, r_dst, 0x02
		beq	word_fill		/* aligned to word? */
half_fill:
		sth	r_val, 0(r_dst)
		addi	r_dst, r_dst, 2
		subi	r_len, r_len, 2		/* subtract halfword */

word_fill:
		cmplwi	r_len, 4		/* we have more than 4 bytes? */
		blt-	trailing_bytes		/* no?  finish writing */
		srwi	r_tmp, r_len, 2		/* get word count */
		mtctr	r_tmp
		subi	r_dst, r_dst, 4
1:		stwu	r_val, 4(r_dst)
		bdnz+	1b

trailing_bytes:
		andi.	r_len, r_len, 0x03	/* how much left? */
		beqlr+				/* nothing? return */
		addi	r_dst, r_dst, 3
		b	simple_fill
