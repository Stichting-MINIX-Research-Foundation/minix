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

RCSID("$NetBSD: strcat_naive.S,v 1.3 2013/08/19 17:02:25 matt Exp $")

ENTRY(strcat)
	mov	ip, r0			/* need to preserve r0 */
#if defined(__thumb__)
1:	ldrb	r2, [r0]		/* load next byte */
	adds	r0, r0, #1		/* advance */
	cmp	r2, #0			/* was it a NUL? */
	bne	1b			/*   no, get next byte */
	subs	r0, r0, #1		/* back up one to the NUL */
	subs	r1, r1, r0		/* save one increment */
2:	ldrb	r2, [r1, r0]		/* load next byte from append */
	strb	r2, [r0]		/* store it */
	adds	r0, r0, #1		/* advance */
	cmp	r2, #0			/* was it a NUL? */
	bne	2b			/*   no, get next byte */
	mov	r0, ip			/* restore dst address */
	RET				/* return */
#else /* !__thumb__ */
	mov	ip, r0			/* need to preserve r0 */
1:	ldrb	r2, [ip], #1		/* load next byte */
	teq	r2, #0			/* was it a NUL? */
	bne	1b			/*   no, get next byte */
	sub	ip, ip, #1		/* back up one to the NUL */
2:	ldrb	r2, [r1], #1		/* load next byte from append */
	strb	r2, [ip], #1		/* store it */
	teq	r2, #0			/* was it a NUL? */
	bne	2b			/*   no, get next byte */
	RET				/* return */
#endif
END(strcat)
