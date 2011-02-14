/*	$NetBSD: atomic_cas.S,v 1.5 2008/05/25 15:56:11 chs Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
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

LEAF(_atomic_cas_32, 3)
1:	mov	a2, t2
	ldl_l	v0, 0(a0)
	cmpeq	v0, a1, t1
	beq	t1, 2f
	stl_c	t2, 0(a0)
	beq	t2, 3f
2:	RET
3:	br	1b
	END(_atomic_cas_32)

ATOMIC_OP_ALIAS(atomic_cas_32,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint,_atomic_cas_32)

ATOMIC_OP_ALIAS(atomic_cas_32_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_32_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint_ni,_atomic_cas_32)

LEAF(_atomic_cas_64, 3)
1:	mov	a2, t2
	ldq_l	v0, 0(a0)
	cmpeq	v0, a1, t1
	beq	t1, 2f
	stq_c	t2, 0(a0)
	beq	t2, 3f
2:	RET
3:	br	1b
	END(_atomic_cas_64)

ATOMIC_OP_ALIAS(atomic_cas_64,_atomic_cas_64)
ATOMIC_OP_ALIAS(atomic_cas_ulong,_atomic_cas_64)
STRONG_ALIAS(_atomic_cas_ulong,_atomic_cas_64)
ATOMIC_OP_ALIAS(atomic_cas_ptr,_atomic_cas_64)
STRONG_ALIAS(_atomic_cas_ptr,_atomic_cas_64)

ATOMIC_OP_ALIAS(atomic_cas_64_ni,_atomic_cas_64)
STRONG_ALIAS(_atomic_cas_64_ni,_atomic_cas_64)
ATOMIC_OP_ALIAS(atomic_cas_ulong_ni,_atomic_cas_64)
STRONG_ALIAS(_atomic_cas_ulong_ni,_atomic_cas_64)
ATOMIC_OP_ALIAS(atomic_cas_ptr_ni,_atomic_cas_64)
STRONG_ALIAS(_atomic_cas_ptr_ni,_atomic_cas_64)
