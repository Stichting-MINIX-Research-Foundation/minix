/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>
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

	.text
ENTRY(__clzdi2)
	movs	r3, r0
	movne	r0, #31
	bne	.L_clz
	movs	r3, r1
	movne	r0, #63
	bne	.L_clz
	mov	r0, #64
	RET
END(__clzdi2)

ENTRY(__clzsi2)
	movs	r3, r0
	moveq	r0, #32
	RETc(eq)
	mov	r0, #31
.L_clz:
	mvn	r1, #0
#ifndef __OPTIMIZE_SIZE__
	eor	r1, r1, r1, lsr #16	/* 0xFFFFFFFF -> 0xFFFF0000 */
	ands	r2, r3, r1	
	eorne	r0, r0, #16
	movne	r3, r2
	eor	r1, r1, r1, lsr #8	/* 0xFFFF0000 -> 0xFF00FF00 */
	ands	r2, r3, r1	
	eorne	r0, r0, #8
	movne	r3, r2
	eor	r1, r1, r1, lsr #4	/* 0xFF00FF00 -> 0xF0F0F0F0 */
	ands	r2, r3, r1	
	eorne	r0, r0, #4
	movne	r3, r2
	eor	r1, r1, r1, lsr #2	/* 0xF0F0F0F0 -> 0xCCCCCCCC */
	ands	r2, r3, r1	
	eorne	r0, r0, #2
	movne	r3, r2
	eor	r1, r1, r1, lsr #1	/* 0xCCCCCCCC -> 0xAAAAAAAA */
	ands	r2, r3, r1	
	eorne	r0, r0, #1
#if 0
	teqeq	r3, #0
	addeq	r0, r0, #1
#endif
#else
	mov	r2, #16
1:	eor	r1, r1, r1, lsr r2
	ands	ip, r3, r1	
	movne	r3, ip
	eorne	r0, r0, r2
	movs	r2, r2, lsr #1
	bne	1b
#if 0
	teq	r3, #0
	addeq	r0, r0, #1
#endif
#endif	/* __OPTIMIZE_SIZE__ */
	RET
END(__clzsi2)
