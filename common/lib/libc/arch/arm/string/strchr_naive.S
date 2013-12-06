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

RCSID("$NetBSD: strchr_naive.S,v 1.4 2013/08/19 17:02:25 matt Exp $")

/* LINTSTUB: char * strchr(const char *, int) */
ENTRY(strchr)
#ifdef __thumb__
	movs	r3, #0xff
	ands	r1, r1, r3	/* restrict to a byte value */
1:	ldrb	r3, [r0]	/* read a byte */
	cmp	r3, r1		/* does it match? */
	beq	2f		/*   yes, set return value */
	adds	r0, r0, #1	/* advance to next byte */
	cmp	r3, #0		/* was it a NUL? */
	bne	1b		/*   no, get next byte */
	movs	r0, #0		/* set return to NULL */
2:	RET			/* return */
#else
	mov	r2, r0		/* using r0 as return value */
	mov	r0, #0		/* default to no match */
	and	r1, r1, #0xff	/* restrict to a byte value */
1:	ldrb	r3, [r2], #1	/* read a byte */
	cmp	r3, r1		/* does it match? */
	subeq	r0, r2, #1	/*   yes, set return value to point to it */
	cmpne	r3, #0		/*   no, was it a NUL? */
	bne	1b		/*     no, get next byte */
	RET
#endif
END(strchr)
