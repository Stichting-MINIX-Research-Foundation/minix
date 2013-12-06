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

RCSID("$NetBSD: strcpy_thumb.S,v 1.1 2013/08/20 21:32:50 matt Exp $")

#ifdef _LIBC
#ifdef STRLCPY
WEAK_ALIAS(strlcpy, _strlcpy)           
#endif
#include "namespace.h"
#endif

/*
 * These are "naive" versions of the str*cpy routines designed to be simple
 * and small.
 */

#if defined(STRLCPY)
/* LINTSTUB: size_t strlcpy(char *, const char *, size_t) */
ENTRY(strlcpy)
	adds	r3, r1, #1		/* save src pointer (+ NUL) */
	subs	r2, r2, #1		/* make sure there's room for a NUL */
	blt	3f			/*   no room, do the strlen */
	adds	r2, r2, r0		/* get end of dst */
	subs	r0, r0, r1		/* bias against r1 so one incr */
1:	cmp	r0, r2			/* room for another char? */
	beq	2f			/*   no, write NUL and do the strlen */
	ldrb	r3, [r1]		/* read a byte */
	strb	r3, [r0, r1]		/* write a byte */
	adds	r1, r1, #1		/* advance to next byte */
	cmp	r3, #0			/* was it a NUL? */
	bne	1b			/*   no, do next byte */
	b	4f			/*   yes, end of string, so return */
2:	movs	r3, #0			/* NUL */
	strb	r3, [r0]		/* write to end of string */
3:	ldrb	r3, [r1]		/* read a byte */
	adds	r1, r1, #1		/* advance to next byte */
	cmp	r3, #0			/* was it a NUL? */
	bne	3b			/*   no, get next byte */
4:	subs	r0, r1, r3 		/* return length of src string */
	RET
END(strlcpy)
#elif defined(STRNCPY)
/* LINTSTUB: char * strncpy(char *, const char *, size_t) */
ENTRY(strncpy)
	mov	ip, r0			/* we want to preserve r0 */
	adds	r2, r2, r0		/* get end of dst buffer */
	subs	r1, r1, r0		/* allows to only increment once */
1:	cmp	r0, r2			/* are at the end of dst already? */
	beq	3f			/*   yes, just return. */
	ldrb	r3, [r1, r0]		/* read a byte */
	strb	r3, [r0]		/* write a byte */
	adds	r0, r0, #1		/* advance a byte */
	cmp	r3, #0			/* was it a NUL? */
	bne	1b			/*   no, try next byte */
2:	cmp	r0, r2			/* are at the end of dst already? */
	beq	3f			/*   yes, exit loop
	strb	r3, [r0]		/* write a NUL */
	adds	r0, r0, #1		/* advance a bytes */
	b	2b			/* until dst is filled */
3:	mov	r0, ip			/* return dst pointer */
	RET
END(strncpy)
#else
/* LINTSTUB: char * strcpy(char *, const char *) */
ENTRY(strcpy)
	subs	r2, r0, r1		/* we want to preserve r0 */
1:	ldrb	r3, [r1]		/* read a byte */
	strb	r3, [r2, r1]		/* write a byte */
	adds	r1, r1, #1		/* advance to next byte */
	cmp	r3, #0			/* was it a NUL? */
	bne	1b			/*   no, try next byte */
	RET				/* return dst pointer */
END(strcpy)
#endif
