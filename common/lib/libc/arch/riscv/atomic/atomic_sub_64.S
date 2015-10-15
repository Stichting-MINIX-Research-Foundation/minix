/*	$NetBSD: atomic_sub_64.S,v 1.2 2015/03/27 06:42:37 matt Exp $	*/

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

ENTRY_NP(_atomic_dec_64)
	li		a1, 1
ENTRY_NP(_atomic_sub_64)
	neg		t1, a1
	amoadd.d.aq	a0, t1, 0(a0)
	ret
END(_atomic_sub_64)
END(_atomic_dec_64)

ATOMIC_OP_ALIAS(atomic_sub_64,_atomic_sub_64)
ATOMIC_OP_ALIAS(atomic_sub_long,_atomic_sub_64)
STRONG_ALIAS(_atomic_sub_long,_atomic_sub_64)
ATOMIC_OP_ALIAS(atomic_sub_ptr,_atomic_sub_64)
STRONG_ALIAS(_atomic_sub_ptr,_atomic_sub_64)
CRT_ALIAS(__sync_fetch_and_sub_8,_atomic_sub_64)
CRT_ALIAS(__atomic_fetch_sub_8,_atomic_sub_64)

ATOMIC_OP_ALIAS(atomic_dec_64,_atomic_dec_64)
ATOMIC_OP_ALIAS(atomic_dec_ulong,_atomic_dec_64)
STRONG_ALIAS(_atomic_dec_ulong,_atomic_dec_64)
ATOMIC_OP_ALIAS(atomic_dec_ptr,_atomic_dec_64)
STRONG_ALIAS(_atomic_dec_ptr,_atomic_dec_64)

ENTRY_NP(_atomic_dec_64_nv)
	li		a1, 1
ENTRY_NP(_atomic_sub_64_nv)
	neg		t1, a1
	amoadd.d.aq	t0, t1, 0(a0)	/* update memory and get old value */
	add		a0, t0, t1	/* turn into new value */
	ret
END(_atomic_sub_64_nv)
END(_atomic_dec_64_nv)

ATOMIC_OP_ALIAS(atomic_sub_64_nv,_atomic_sub_64_nv)
ATOMIC_OP_ALIAS(atomic_sub_long_nv,_atomic_sub_64_nv)
STRONG_ALIAS(_atomic_sub_long_nv,_atomic_sub_64_nv)
ATOMIC_OP_ALIAS(atomic_sub_ptr_nv,_atomic_sub_64_nv)
STRONG_ALIAS(_atomic_sub_ptr_nv,_atomic_sub_64_nv)
CRT_ALIAS(__sync_sub_and_fetch_8,_atomic_sub_64_nv)
CRT_ALIAS(__atomic_sub_fetch_8,_atomic_sub_64_nv)

ATOMIC_OP_ALIAS(atomic_dec_64_nv,_atomic_dec_64_nv)
ATOMIC_OP_ALIAS(atomic_dec_ulong_nv,_atomic_dec_64_nv)
STRONG_ALIAS(_atomic_dec_ulong_nv,_atomic_dec_64_nv)
ATOMIC_OP_ALIAS(atomic_dec_ptr_nv,_atomic_sub_64_nv)
STRONG_ALIAS(_atomic_sub_ptr_nv,_atomic_sub_64_nv)
