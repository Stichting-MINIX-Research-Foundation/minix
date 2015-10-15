/*	$NetBSD: atomic_cas_32.S,v 1.7 2014/03/04 16:15:28 matt Exp $	*/
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
ENTRY_NP(_atomic_cas_32)
	mov	ip, r0			/* we need r0 for return value */
1:
	ldrex	r0, [ip]		/* load old value */
	cmp	r0, r1			/*   compare? */
#ifdef __thumb__
	bne	2f			/*     return if different */
#else
	RETc(ne)			/*     return if different */
#endif
	strex	r3, r2, [ip]		/* store new value */
	cmp	r3, #0			/*   succeed? */
	bne	1b			/*     nope, try again. */
#ifdef _ARM_ARCH_7
	dsb
#else
	mcr	p15, 0, r3, c7, c10, 4	/* data synchronization barrier */
#endif
2:	RET				/* return. */
END(_atomic_cas_32)

ATOMIC_OP_ALIAS(atomic_cas_32,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ulong,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ptr,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_32_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ulong_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ptr_ni,_atomic_cas_32)
CRT_ALIAS(__sync_val_compare_and_swap_4,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_32_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ptr_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ptr,_atomic_cas_32)

#endif /* _ARM_ARCH_6 */
