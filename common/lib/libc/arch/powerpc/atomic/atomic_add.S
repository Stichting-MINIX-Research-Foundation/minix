/*	$NetBSD: atomic_add.S,v 1.8 2014/03/07 07:17:54 matt Exp $	*/

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

__RCSID("$NetBSD: atomic_add.S,v 1.8 2014/03/07 07:17:54 matt Exp $")

	.text
ATOMIC_OP_32(add)
ATOMIC_OP_32_NV(add)

ATOMIC_OP_ALIAS(atomic_add_int,_atomic_add_32)
ATOMIC_OP_ALIAS(atomic_add_int_nv,_atomic_add_32_nv)

STRONG_ALIAS(_atomic_add_int,_atomic_add_32)
STRONG_ALIAS(_atomic_add_int_nv,_atomic_add_32_nv)

#ifdef _LP64
ATOMIC_OP_64(add)
ATOMIC_OP_64_NV(add)

ATOMIC_OP_ALIAS(atomic_add_long,_atomic_add_64)
ATOMIC_OP_ALIAS(atomic_add_long_nv,_atomic_add_64_nv)

ATOMIC_OP_ALIAS(atomic_add_ptr,_atomic_add_64)
ATOMIC_OP_ALIAS(atomic_add_ptr_nv,_atomic_add_64_nv)

STRONG_ALIAS(_atomic_add_long,_atomic_add_64)
STRONG_ALIAS(_atomic_add_long_nv,_atomic_add_64_nv)

STRONG_ALIAS(_atomic_add_ptr,_atomic_add_64)
STRONG_ALIAS(_atomic_add_ptr_nv,_atomic_add_64_nv)
#else
ATOMIC_OP_ALIAS(atomic_add_long,_atomic_add_32)
ATOMIC_OP_ALIAS(atomic_add_long_nv,_atomic_add_32_nv)

ATOMIC_OP_ALIAS(atomic_add_ptr,_atomic_add_32)
ATOMIC_OP_ALIAS(atomic_add_ptr_nv,_atomic_add_32_nv)

STRONG_ALIAS(_atomic_add_long,_atomic_add_32)
STRONG_ALIAS(_atomic_add_long_nv,_atomic_add_32_nv)

STRONG_ALIAS(_atomic_add_ptr,_atomic_add_32)
STRONG_ALIAS(_atomic_add_ptr_nv,_atomic_add_32_nv)
#endif
