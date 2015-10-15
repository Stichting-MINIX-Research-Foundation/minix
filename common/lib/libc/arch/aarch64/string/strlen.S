/* $NetBSD: strlen.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

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

RCSID("$NetBSD: strlen.S,v 1.1 2014/08/10 05:47:35 matt Exp $")

#ifdef STRNLEN
#define FUNCNAME	strnlen
/* LINTSTUB: size_t strnlen(const char *, size_t); */
#else
#define FUNCNAME	strlen
/* LINTSTUB: size_t strlen(const char *); */
#endif

#define	MASK8_0x01	0x0101010101010101
#define	MASK8_0x7f	0x7f7f7f7f7f7f7f7f

ENTRY(FUNCNAME)
	mov	x4, x0			/* need x0 for return */
	add	x9, x0, #8		/* start + dword */
#ifdef STRNLEN
	add	x10, x0, x1		/* don't go past here */
#endif
	mov	x11, #MASK8_0x01	/* test mask */

	ands	x3, x4, #7		/* extract alignment */
	neg	x0, x3			/* alignment fixup */
	b.eq	.Lstrlen_dword_loop	/* already dword aligned */

	/*
	 * Load the dword containing the leading bytes.  Make sure bytes
	 * before the data won't match as NUL.
	 */
	add	x4, x4, x0		/* make dword aligned */
	ldr	x7, [x4], #8		/* load dword */
	lsl	x3, x3, #3		/* convert bytes to bits */
#ifdef __AARCH64EB__
	lsr	x5, x11, x3		/* make mask for BE */
#else
	lsl	x5, x11, x3		/* make mask for LE */
#endif
	eor	x5, x5, x11		/* invert mask */
	orr	x7, x7, x5		/* prevent NULs */
	b	.Lstrlen_dword_loop_noload

.Lstrlen_dword_loop:
#ifdef STRNLEN
	cmp	x4, x10
	b.ge	.Lstrlen_done
#endif
	ldr	x7, [x4], #8		/* load dword */
.Lstrlen_dword_loop_noload:
	/*
	 * Use the formula (X - 1) & ~(X | 0x7f) to find NUL bytes.
	 * Any NUL byte found will be replaced by 0x80 otherwise any byte
	 * will be replaced by 0x00.
	 */
	sub	x6, x7, x11		/* a = X - 1 */
	orr	x7, x7, #MASK8_0x7f	/* b = X | 0x7f */
	bic	x6, x6, x7		/* a & ~b */
	cbz	x6, .Lstrlen_dword_loop	/* no NULs so get next dword */

	/*
	 * We know there is a NUL in this dword.  Use clz to find it.
	 */
#ifdef __AARCH64EL__
	rev	x7, x7			/* convert to BE */
#endif
	clz	x7, x7			/* find null byte */
	add	x0, x0, x7, lsr #3	/* add offset to the length */

	add	x0, x0, x4		/* add end to the length */
	sub	x0, x0, x9		/* subtract start from the length */
#ifdef STRNLEN
	cmp	x0, x1			/* did we go too far? */
        csel    x0, x0, x1, lt		/* yes, return max length */
#endif
	ret
#ifdef STRNLEN
.Lstrlen_done:
	mov	x0, x1
	ret
#endif
END(FUNCNAME)
