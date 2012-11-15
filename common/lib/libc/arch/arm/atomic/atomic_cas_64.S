/*	$NetBSD: atomic_cas_64.S,v 1.1 2012/09/11 20:51:25 matt Exp $	*/
/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include "atomic_op_asm.h"

#if defined(_ARM_ARCH_6)
/*
 * ARMv6 has load-exclusive/store-exclusive which works for both user
 * and kernel.
 */
ENTRY_NP(_atomic_cas_64)
	stmfd	sp!, {r4, r5, r6}	/* save temporaries */
	mov	r6, r0			/* we need r0 for return value */
#ifdef __ARM_EABI__
	ldrd	r4, [sp]		/* fetch new value */
#else
	ldr	r5, [sp, #4]		/* fetch new value */
	ldr	r4, [sp, #0]		/* fetch new value */
	mov	r3, r2			/* r2 will be overwritten by r1 which ... */
	mov	r2, r1			/* r1 will be overwritten by ldrexd */
#endif
1:
	ldrexd	r0, [r6]		/* load current value */
	teq	r0, r2			/*   compare to old? 1st half */
	teqeq	r1, r3			/*   compare to old? 2nd half */
	bne	2f			/*     jump to return if different */
	strexd	ip, r4, [r6]		/* store new value */
	cmp	ip, #0			/*   succeed? */
	bne	1b			/*     nope, try again. */
#ifdef _ARM_ARCH_7
	dsb
#else
	mcr	p15, 0, ip, c7, c10, 4	/* data synchronization barrier */
#endif
2:
	ldmfd	sp!, {r4, r5, r6}	/* restore temporaries */
	RET				/* return. */
	END(_atomic_cas_64)

ATOMIC_OP_ALIAS(atomic_cas_64,_atomic_cas_64)

#endif /* _ARCH_ARM_6 */
