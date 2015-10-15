/* $NetBSD: atomic_swap_8.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

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

#include "atomic_op_asm.h"

ENTRY_NP(_atomic_swap_8)
	mov	x4, x0
1:	ldxrb	w0, [x4]
	stxrb	w3, w1, [x4]
	cbnz	w3, 1b
	dmb	st
	ret
END(_atomic_swap_8)

ATOMIC_OP_ALIAS(atomic_swap_8,_atomic_swap_8)
ATOMIC_OP_ALIAS(atomic_swap_char,_atomic_swap_8)
ATOMIC_OP_ALIAS(atomic_swap_uchar,_atomic_swap_8)
STRONG_ALIAS(__sync_lock_test_and_set_1,_atomic_swap_8)
STRONG_ALIAS(_atomic_swap_char,_atomic_swap_8)
STRONG_ALIAS(_atomic_swap_uchar,_atomic_swap_8)

ENTRY_NP(__sync_lock_release_1)
	stlrb	wzr, [x0]
	ret
END(__sync_lock_release_1)
