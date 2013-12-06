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

RCSID("$NetBSD: strlcat_naive.S,v 1.3 2013/08/22 19:25:00 matt Exp $")

/* LINTSTUB: size_t strlcat(char *, const char *, size_t) */
ENTRY(strlcat)
	adds	r3, r2, r0		/* point to just end of dst */
	mov	ip, r0			/* need to keep r0 for a while */
1:	cmp	ip, r3			/* still within dst? */
	beq	3f			/*   no, get length of src */
	ldrb	r2, [ip], #1		/* load next byte */
	cmp	r2, #0			/* was it a NUL? */
	bne	1b			/*   no, get next byte */

	sub	r0, ip, r0		/* get actual length of dst */
	subs	r0, r0, #1		/* account for the trailing NUL */
	subs	r3, r3, #1		/* back up to last byte in dst */
	sub	ip, ip, #1		/* back up over the NUL */

2:	ldrb	r2, [r1], #1		/* load next byte from append */
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r2, 5f			/* was it a NUL? yes, end of append */
#else
	cmp	r2, #0			/* was it a NUL? */
	beq	5f			/*   yes, end of append */
#endif
	adds	r0, r0, #1		/* count another byte */
	cmp	ip, r3			/* do we have enough room for it? */
#ifdef __thumb__
	bge	2b
	strb	r2, [ip], #1		/*   yes, store it */
#else
	strblt	r2, [ip], #1		/*   yes, store it */
#endif
	b	2b			/* get next byte from append */

3:	subs	r0, r3, r0		/* move size to return value */
4:	ldrb	r2, [r1], #1		/* load next byte from append */
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r2, 6f			/* was it a NUL? yes, we're done */
#else
	RETc(eq)			/*   yes, we're done */
#endif
	adds	r0, r0, #1		/* add one to return value */
	b	4b			/* get next byte from append */

5:	movs	r2, #0			/* NUL */
	strb	r2, [ip]		/* append final NUL */
6:	RET				/* return */
END(strlcat)
