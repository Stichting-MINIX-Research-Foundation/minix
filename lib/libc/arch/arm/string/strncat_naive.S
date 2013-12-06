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

RCSID("$NetBSD: strncat_naive.S,v 1.1 2013/01/23 05:44:52 matt Exp $")

/* LINTSTUB: char *strncat(char *, const char *, size_t) */
ENTRY(strncat)
	teq	r2, #0			/* count is 0? */
	RETc(eq)			/*   yes, just return dst */
	mov	ip, r0			/* need to preserve r0 */
1:	ldrb	r3, [ip], #1		/* load next byte */
	teq	r3, #0			/* was it a NUL? */
	bne	1b			/*   no, get next byte */
	sub	ip, ip, #1		/* back up one to the NUL */
2:	subs	r2, r2, #1		/* subtract 1 from the count */
	blt	3f			/*   <0? write terminating NUL */
	ldrb	r3, [r1], #1		/* load next byte from append */
	strb	r3, [ip], #1		/* store it */
	teq	r3, #0			/* was it a NUL? */
	bne	2b			/*   no, get next byte */
	RET				/* return */
3:	mov	r3, #0			/* load a NUL */
	strb	r3, [ip]		/* store it */
	RET				/* return */
END(strncat)
