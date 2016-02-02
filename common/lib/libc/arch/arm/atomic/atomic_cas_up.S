/*	$NetBSD: atomic_cas_up.S,v 1.7 2014/03/04 03:36:24 matt Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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

#include <sys/ras.h>
#include <machine/asm.h>

ENTRY(_atomic_cas_up)
	.hidden	_C_LABEL(_atomic_cas_up)
	mov	r3, r0
	.align	0
RAS_START_ASM_HIDDEN(_atomic_cas)
	ldr	r0, [r3]
	cmp	r0, r1
#if defined(__thumb__)
	beq	1f
#else
	streq	r2, [r3]
#endif
	.align	0
RAS_END_ASM_HIDDEN(_atomic_cas)
1:	RET
END(_atomic_cas_up)

#if defined(__HAVE_ASM_ATOMIC_CAS_64_UP)
ARM_ENTRY(_atomic_cas_64_up)
	push	{r4-r5}
	mov	ip, r0
	ldrd	r4, r5, [sp, #8]
	.align	0
RAS_START_ASM_HIDDEN(_atomic_cas_64)
	ldrd	r0, r1, [ip]
	cmp	r0, r2
	cmpeq	r1, r3
	strdeq	r4, r5, [ip]
	.align	0
RAS_END_ASM_HIDDEN(_atomic_cas_64)
1:	pop	{r4-r5}
	bx	lr
END(_atomic_cas_64_up)
#endif /* __HAVE_ASM_ATOMIC_64_UP */

ENTRY(_atomic_cas_16_up)
	mov	r3, r0
	.align	0
RAS_START_ASM_HIDDEN(_atomic_cas_16)
	ldrh	r0, [r3]
	cmp	r0, r1
#if defined(__thumb__)
	beq	1f
#else
	strheq	r2, [r3]
#endif
	.align	0
RAS_END_ASM_HIDDEN(_atomic_cas_16)
1:	RET
END(_atomic_cas_16_up)

ENTRY(_atomic_cas_8_up)
	mov	r3, r0
	.align	0
RAS_START_ASM_HIDDEN(_atomic_cas_8)
	ldrb	r0, [r3]
	cmp	r0, r1
#if defined(__thumb__)
	beq	1f
#else
	strbeq	r2, [r3]
#endif
	.align	0
RAS_END_ASM_HIDDEN(_atomic_cas_8)
1:	RET
END(_atomic_cas_8_up)
