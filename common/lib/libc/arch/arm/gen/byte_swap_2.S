/*	$NetBSD: byte_swap_2.S,v 1.6 2013/08/19 03:44:18 matt Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
#define FUNC _C_LABEL(bswap16)
#else
#define FUNC _C_LABEL(__bswap16)
#endif

ENTRY(FUNC)
#ifdef _ARM_ARCH_6
	rev16		r0, r0
#elif !defined(__thumb__)
	and		r1, r0, #0xff
	mov		r0, r0, lsr #8
	orr		r0, r0, r1, lsl #8
#else
	movs		r2, #0xff
	movs		r1, r0
	ands		r1, r1, r2
	lsls		r1, r1, #8
	lsrs		r0, r0, #8
	ands		r0, r0, r2
	orrs		r0, r0, r1
#endif
	RET
END(FUNC)
#if BYTE_ORDER == LITTLE_ENDIAN
STRONG_ALIAS(_C_LABEL(ntohs), FUNC)
STRONG_ALIAS(_C_LABEL(htons), FUNC)
#endif
