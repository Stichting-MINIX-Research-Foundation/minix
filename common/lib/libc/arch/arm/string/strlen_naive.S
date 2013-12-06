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

RCSID("$NetBSD: strlen_naive.S,v 1.8 2013/08/19 17:02:25 matt Exp $")

#ifdef STRNLEN
/* LINTSTUB: size_t strnlen(const char *, size_t) */
ENTRY(strnlen)
	adds	r3, r0, #1	/* start of src + NUL */
#ifdef __thumb__
	adds	r2, r0, r1	/* &src[maxlen] */
	mov	ip, r2
#else
	adds	ip, r0, r1	/* &src[maxlen] */
#endif
1:	cmp	r0, ip		/* is this of string? */
	beq	2f		/*   yes it is */
#ifdef __thumb__
	ldrb	r2, [r0]	/* read a byte */
	adds	r0, r0, #1	/* advance to next byte */
#else
	ldrb	r2, [r0], #1	/* read a byte */
#endif
	cmp	r2, #0		/* is it a NUL? */
	bne	1b		/*   no, get next byte */
	subs	r0, r0, r3	/* get difference between start and end */
	RET			/* return */
2:	mov	r0, r1		/* get maxlen */
	RET			/* return */
END(strnlen)
#else /* STRNLEN */
/* LINTSTUB: size_t strlen(const char *) */
ENTRY(strlen)
	adds	r3, r0, #1	/* start of src + NUL */
1:
#ifdef __thumb__
	ldrb	r2, [r0]	/* read a byte */
	adds	r0, r0, #1
#else
	ldrb	r2, [r0], #1	/* read a byte */
#endif
	cmp	r2, #0		/* is it a NUL? */
	bne	1b		/*   no, get next byte */
	subs	r0, r0, r3	/* return difference between start and end */
	RET
END(strlen)
#endif /* !STRNLEN */
