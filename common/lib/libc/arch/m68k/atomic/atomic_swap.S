/*	$NetBSD: atomic_swap.S,v 1.9 2014/06/28 20:18:55 joerg Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

	.text

ENTRY(_atomic_swap_32)
	movl	4(%sp), %a0
1:	movl	(%a0), %d0
	movl	8(%sp), %d1
	casl	%d0, %d1, (%a0)
	bne	1b
	/* %d0 now contains the old value */
	movl	%d0, %a0	/* pointers return also in %a0 */
	rts
END(_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_32,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_uint,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_uint,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_ulong,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ulong,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_ptr,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ptr,_atomic_swap_32)
CRT_ALIAS(__sync_lock_test_and_set_4,_atomic_swap_32)
CRT_ALIAS(__atomic_exchange_4,_atomic_swap_32)


ENTRY(_atomic_swap_16)
	movl	4(%sp), %a0
1:	movw	(%a0), %d0
	movw	8(%sp), %d1
	casw	%d0, %d1, (%a0)
	bne	1b
	/* %d0 now contains the old value */
	rts
END(_atomic_swap_16)
ATOMIC_OP_ALIAS(atomic_swap_16,_atomic_swap_16)
CRT_ALIAS(__sync_lock_test_and_set_2,_atomic_swap_16)
CRT_ALIAS(__atomic_exchange_2,_atomic_swap_16)

ENTRY(_atomic_swap_8)
	movl	4(%sp), %a0
1:	movb	(%a0), %d0
	movb	8(%sp), %d1
	casb	%d0, %d1, (%a0)
	bne	1b
	/* %d0 now contains the old value */
	rts
END(_atomic_swap_8)
ATOMIC_OP_ALIAS(atomic_swap_8,_atomic_swap_8)
CRT_ALIAS(__sync_lock_test_and_set_1,_atomic_swap_8)
CRT_ALIAS(__atomic_exchange_1,_atomic_swap_8)
