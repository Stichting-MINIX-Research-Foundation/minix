/* $NetBSD: atomic_cas_8.S,v 1.3 2012/08/31 23:41:52 matt Exp $ */
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

#include "atomic_op_asm.h"

#if defined(_ARM_ARCH_6)
/*
 * ARMv6 has load-exclusive/store-exclusive which works for both user
 * and kernel.
 */
ENTRY_NP(_atomic_cas_8)
	mov	r3, r0			/* we need r0 for return value */
1:
	ldrexb	r0, [r3]		/* load old value */
	teq	r0, r1			/*   compare? */
	RETc(ne)			/*     return if different */
	strexb	ip, r2, [r3]		/* store new value */
	cmp	ip, #0			/*   succeed? */
	bne	1b			/*     nope, try again. */
#ifdef _ARM_ARCH_7
	dsb				/* data synchronization barrier */
#else
	mcr	p15, 0, ip, c7, c10, 4	/* data synchronization barrier */
#endif
	RET				/* return. */
	END(_atomic_cas_8)

ATOMIC_OP_ALIAS(atomic_cas_8,_atomic_cas_8)
STRONG_ALIAS(_atomic_cas_char,_atomic_cas_8)
STRONG_ALIAS(_atomic_cas_uchar,_atomic_cas_8)

#endif /* _ARCH_ARM_6 */
