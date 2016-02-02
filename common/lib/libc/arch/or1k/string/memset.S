/*	$NetBSD: memset.S,v 1.1 2014/09/03 19:34:25 matt Exp $ */

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


#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: memset.S,v 1.1 2014/09/03 19:34:25 matt Exp $")
#endif /* LIBC_SCCS && !lint */

/*----------------------------------------------------------------------*/
/*
     void bzero(void *b r3, size_t len r4);
     void * memset(void *b r3, int c r4, size_t len r5);
*/
/*----------------------------------------------------------------------*/


#ifdef _BZERO
#define r_fill	r0
ENTRY(bzero)
#else
#define r_fill	r4
ENTRY(memset)
#endif
#ifdef _BZERO
		l.ori	r5, r4, 0
		l.or	r4, r0, r0
#else
		l.or	r11, r3, r0		/* move start to return value */
#endif
		l.sfeqi	r5, 0			/* anything to do? */
		l.bf	.Lret			/*   no, just return */
		l.nop

		l.sfgeui r5, 7			/* small buffer? */
		l.add	r5, r5, r3		/* r5 is end pointer */
		l.bnf	.Lbyte_fill		/*   yes.  just byte fill */
		l.nop

#ifndef _BZERO
		// Let's see the fill type
		l.sfeqi	r4, 0			/* filling with 0? */
		l.bf	.Lalignment_check	/* don't to replicate */
		l.nop
		l.extbz	r4, r4			/* truncate to 8 bits */
		l.slli	r13, r4, 8		/* shift left 8 bits */
		l.or	r4, r4, r13		/* merge the two bytes */
		l.slli	r13, r4, 16		/* shift left 16 bits */
		l.or	r4, r4, r13		/* merge the two halves */

.Lalignment_check:
#endif
		l.andi	r13, r3, 3		/* get low bits of start */
		l.sfeqi	r13, 0			/* word aligned? */
		l.bf	.Lword_fill		/*   yes, start setting */
		l.nop

		l.add	r5, r5, r13		/* increase length */
		l.sub	r3, r3, r13		/* mask word aligned */
		l.slli	r13, r13, 3		/* bytes to bits */
		l.addi	r15, r13, -8		/* minus one byte */

		l.lwz	r6, 0(r3)		/* get first word */
		l.movhi	r7, 0xff00		/* 0xff000000 */
		l.sra	r7, r7, r13		/* shift right align bytes */
		l.and	r6, r6, r7		/* clear bytes to be filled */
#ifndef _BZERO
		l.srl	r7, r_fill, r13		/* clear bytes to preserve */
		l.or	r6, r6, r7		/* merge existing with new */
#endif
		l.sw	0(r3), r6		/* store first word */
		l.addi	r3, r3, 4		/* advance to next word */
		l.addi	r5, r5, -4		/* one less word to do */
		
.Lword_aligned:
		l.srli	r6, r5, 2		/* clear low two bits of len */
		l.srli	r6, r6, 2		/* ... */
		l.sfgeu	r3, r6			/* any more full words? */
		l.bf	.Lend_fill		/* no, handle the last bytes */
		l.nop

.Lword_fill:
		l.sw	0(r3), r_fill		/* store a word */
		l.addi	r3, r3, 4		/* advance */
		l.sfgeu	r3, r6			/* any more full words? */
		l.bnf	.Lword_fill		/*   yes, fill next word */
		l.nop
		l.j	.Lend_fill		/* fill any leftover bytes */
		l.nop

.Lbyte_fill:
		l.sb	0(r3), r_fill		/* store a byte */
		l.addi	r3, r3, 1		/* advance */
.Lend_fill:
		l.sfeq	r3, r5			/* at the end? */
		l.bnf	.Lbyte_fill		/*   no, fill next byte */
		l.nop

.Lret:
		l.jr	lr			/* return */
		l.nop
#ifdef _BZERO
END(bzero)
#else
END(memset)
#endif
