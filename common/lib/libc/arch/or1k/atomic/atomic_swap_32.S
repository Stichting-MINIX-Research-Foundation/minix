/*	$NetBSD: atomic_swap_32.S,v 1.1 2014/09/03 19:34:25 matt Exp $	*/

/*-
 * Copyright (c) 2007,2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Matt Thomas.
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

ENTRY_NP(_atomic_swap_32)
1:	l.lwa	r11, 0(r3)
	l.swa	0(r3), r4
	l.bnf	1b
	l.nop
2:	l.jr	lr
	l.nop
END(_atomic_swap_32)

ATOMIC_OP_ALIAS(atomic_swap_32,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_uint,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_ulong,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_ptr,_atomic_swap_32)
CRT_ALIAS(__sync_lock_test_and_set_4,_atomic_swap_32)
CRT_ALIAS(__atomic_exchange_4,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_uint,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ulong,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ptr,_atomic_swap_32)

#if (!defined(_KERNEL) || defined(_RUMPKERNEL)) && !defined(_STANDALONE)
ENTRY_NP(__sync_lock_release_4)
	l.sw	0(r3), r0
	l.jr	lr
	l.nop
END(__sync_lock_release_4)
#endif
