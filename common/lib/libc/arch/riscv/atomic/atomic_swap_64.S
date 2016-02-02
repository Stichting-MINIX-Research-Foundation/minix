/* $NetBSD: atomic_swap_64.S,v 1.2 2015/03/27 06:42:37 matt Exp $ */

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

#ifdef _LP64
#include "atomic_op_asm.h"

ENTRY_NP(_atomic_swap_64)
	amoswap.d.aq	a0, a1, 0(a0)
	ret
END(_atomic_swap_64)

ATOMIC_OP_ALIAS(atomic_swap_64,_atomic_swap_64)
ATOMIC_OP_ALIAS(atomic_swap_ulong,_atomic_swap_64)
ATOMIC_OP_ALIAS(atomic_swap_ptr,_atomic_swap_64)
STRONG_ALIAS(_atomic_swap_ulong,_atomic_swap_64)
STRONG_ALIAS(_atomic_swap_ptr,_atomic_swap_64)
CRT_ALIAS(__sync_lock_test_and_set_8,_atomic_swap_64)
CRT_ALIAS(__atomic_exchange_8,_atomic_swap_64)

ENTRY_NP(_atomic_release_64)
	amoswap.d.rl	zero, zero, 0(a0)
	ret
END(_atomic_release_64)
ATOMIC_OP_ALIAS(atomic_release_64,_atomic_release_64)

CRT_ALIAS(__sync_lock_release_8,_atomic_release_64)
#endif /* _LP64 */
