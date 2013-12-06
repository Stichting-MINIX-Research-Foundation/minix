/*	$NetBSD: byte_swap_4.S,v 1.7 2013/08/19 03:44:47 matt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Neil A. Carson
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

#if defined(_KERNEL) || defined(_STANDALONE)
#define	FUNC	_C_LABEL(bswap32)
#else
#define	FUNC	_C_LABEL(__bswap32)
#endif
ENTRY(FUNC)
#ifdef _ARM_ARCH_6
	rev	r0, r0
#elif !defined(__thumb__)
	eor	r1, r0, r0, ror #16	/* 4.3.2.1 -> 42.31.42.31 */
	bic	r1, r1, #0x00FF0000	/* 42.31.42.31 -> 42.0.42.31 */
	mov	r0, r0, ror #8		/* 4.3.2.1 -> 1.4.3.2 */
	eor	r0, r0, r1, lsr #8	/* 1.4.3.2 ^ 0.42.0.42 -> 1.2.3.4 */
#else
	movs	r3, #16
	lsls	r1, r0, #8		/* 4.3.2.1 -> 3.2.1.0 /*
	lsrs	r0, r0, #8		/* 4.3.2.1 -> 0.4.3.2 */
	rors	r1, r3			/* 3.2.1.0 -> 1.0.3.2 */
	rors	r0, r3			/* 0.4.3.2 -> 3.2.0.4 */
	lsrs	r1, r1, #8		/* 1.0.3.2 -> 0.1.0.3 */
	lsls	r1, r1, #8		/* 0.1.0.3 -> 1.0.3.0 */
	lsls	r0, r0, #8		/* 3.2.0.4 -> 2.0.4.0 */
	lsrs	r0, r0, #8		/* 2.0.4.0 -> 0.2.0.4 */
	orrs	r0, r0, r1		/* 1.0.3.0 | 0.2.0.4 -> 1.2.3.4 */
#endif
	RET
END(FUNC)

#if BYTE_ORDER == LITTLE_ENDIAN
STRONG_ALIAS(_C_LABEL(ntohl), FUNC)
STRONG_ALIAS(_C_LABEL(htonl), FUNC)
#endif
