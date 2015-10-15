/*	$NetBSD: atomic_swap_16.S,v 1.4 2015/05/17 20:57:11 justin Exp $ */

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

#include "atomic_op_asm.h"

#ifdef _ARM_ARCH_6
ENTRY_NP(_atomic_swap_16)
	mov	ip, r0
1:
	ldrexh	r0, [ip]
	strexh	r3, r1, [ip]
	cmp	r3, #0
	bne	1b
#ifdef _ARM_ARCH_7
	dmb
#else
	mcr	p15, 0, ip, c7, c10, 5	/* data memory barrier */
#endif
	RET
END(_atomic_swap_16)

ATOMIC_OP_ALIAS(atomic_swap_16,_atomic_swap_16)
ATOMIC_OP_ALIAS(atomic_swap_short,_atomic_swap_16)
ATOMIC_OP_ALIAS(atomic_swap_ushort,_atomic_swap_16)
CRT_ALIAS(__sync_lock_test_and_set_2,_atomic_swap_16)
CRT_ALIAS(__atomic_exchange_2,_atomic_swap_16)
STRONG_ALIAS(_atomic_swap_short,_atomic_swap_16)
STRONG_ALIAS(_atomic_swap_ushort,_atomic_swap_16)

#if (!defined(_KERNEL) || !defined(_RUMPKERNEL)) && !defined(_STANDALONE)
ENTRY_NP(__sync_lock_release_2)
	mov	r1, #0
	strh	r1, [r0]
	RET
END(__sync_lock_release_2)
#endif
#endif /* _ARM_ARCH_6 */
