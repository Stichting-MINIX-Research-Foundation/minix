/*	$NetBSD: atomic_add_32.S,v 1.2 2015/03/27 06:42:37 matt Exp $	*/

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

ENTRY_NP(_atomic_inc_32)
	li		a1, 1
ENTRY_NP(_atomic_add_32)
	amoadd.w.aq	a0, a1, 0(a0)
	ret
END(_atomic_add_32)
END(_atomic_inc_32)

ATOMIC_OP_ALIAS(atomic_add_32,_atomic_add_32)
ATOMIC_OP_ALIAS(atomic_add_int,_atomic_add_32)
STRONG_ALIAS(_atomic_add_int,_atomic_add_32)
#ifndef _LP64
ATOMIC_OP_ALIAS(atomic_add_long,_atomic_add_32)
STRONG_ALIAS(_atomic_add_long,_atomic_add_32)
ATOMIC_OP_ALIAS(atomic_add_ptr,_atomic_add_32)
STRONG_ALIAS(_atomic_add_ptr,_atomic_add_32)
#endif
CRT_ALIAS(__sync_fetch_and_add_4,_atomic_add_32)
CRT_ALIAS(__atomic_fetch_add_4,_atomic_add_32)

ATOMIC_OP_ALIAS(atomic_inc_32,_atomic_inc_32)
ATOMIC_OP_ALIAS(atomic_inc_uint,_atomic_inc_32)
STRONG_ALIAS(_atomic_inc_uint,_atomic_inc_32)
#ifndef _LP64
ATOMIC_OP_ALIAS(atomic_inc_ulong,_atomic_inc_32)
STRONG_ALIAS(_atomic_inc_ulong,_atomic_inc_32)
ATOMIC_OP_ALIAS(atomic_inc_ptr,_atomic_inc_32)
STRONG_ALIAS(_atomic_inc_ptr,_atomic_inc_32)
#endif

ENTRY_NP(_atomic_inc_32_nv)
	li		a1, 1
ENTRY_NP(_atomic_add_32_nv)
	amoadd.w.aq	t0, a1, 0(a0)	/* update memory and get old value */
	INT_ADD		a0, t0, a1	/* turn into new value */
	ret
END(_atomic_add_32_nv)
END(_atomic_inc_32_nv)

ATOMIC_OP_ALIAS(atomic_add_32_nv,_atomic_add_32_nv)
ATOMIC_OP_ALIAS(atomic_add_int_nv,_atomic_add_32_nv)
STRONG_ALIAS(_atomic_add_int_nv,_atomic_add_32_nv)
#ifndef _LP64
ATOMIC_OP_ALIAS(atomic_add_long_nv,_atomic_add_32_nv)
STRONG_ALIAS(_atomic_add_long_nv,_atomic_add_32_nv)
ATOMIC_OP_ALIAS(atomic_add_ptr_nv,_atomic_add_32_nv)
STRONG_ALIAS(_atomic_add_ptr_nv,_atomic_add_32_nv)
#endif
CRT_ALIAS(__sync_add_and_fetch_4,_atomic_add_32_nv)
CRT_ALIAS(__atomic_add_fetch_4,_atomic_add_32_nv)

ATOMIC_OP_ALIAS(atomic_inc_32_nv,_atomic_inc_32_nv)
ATOMIC_OP_ALIAS(atomic_inc_uint_nv,_atomic_inc_32_nv)
STRONG_ALIAS(_atomic_inc_uint_nv,_atomic_inc_32_nv)
#ifndef _LP64
ATOMIC_OP_ALIAS(atomic_inc_ulong_nv,_atomic_inc_32_nv)
STRONG_ALIAS(_atomic_inc_ulong_nv,_atomic_inc_32_nv)
ATOMIC_OP_ALIAS(atomic_inc_ptr_nv,_atomic_inc_32_nv)
STRONG_ALIAS(_atomic_inc_ptr_nv,_atomic_inc_32_nv)
#endif
