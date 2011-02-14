/*	$NetBSD: atomic_dec.S,v 1.4 2008/05/25 15:56:11 chs Exp $	*/

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

ENTRY(_atomic_dec_32)
	movl	%sp@(4), %a0
	subql	#1, %a0@
	rts
ATOMIC_OP_ALIAS(atomic_dec_32,_atomic_dec_32)
ATOMIC_OP_ALIAS(atomic_dec_uint,_atomic_dec_32)
STRONG_ALIAS(_atomic_dec_uint,_atomic_dec_32)
ATOMIC_OP_ALIAS(atomic_dec_ulong,_atomic_dec_32)
STRONG_ALIAS(_atomic_dec_ulong,_atomic_dec_32)
ATOMIC_OP_ALIAS(atomic_dec_ptr,_atomic_dec_32)
STRONG_ALIAS(_atomic_dec_ptr,_atomic_dec_32)

ENTRY(_atomic_dec_32_nv)
	movl	%sp@(4), %a0
1:	movl	%a0@, %d0
	movl	%d0, %d1
	subql	#1, %d0
	casl	%d1, %d0, %a0@
	bne	1b
	movl	%d0, %a0	/* pointers return also in %a0 */
	rts
ATOMIC_OP_ALIAS(atomic_dec_32_nv,_atomic_dec_32_nv)
ATOMIC_OP_ALIAS(atomic_dec_uint_nv,_atomic_dec_32_nv)
STRONG_ALIAS(_atomic_dec_uint_nv,_atomic_dec_32_nv)
ATOMIC_OP_ALIAS(atomic_dec_ulong_nv,_atomic_dec_32_nv)
STRONG_ALIAS(_atomic_dec_ulong_nv,_atomic_dec_32_nv)
ATOMIC_OP_ALIAS(atomic_dec_ptr_nv,_atomic_dec_32_nv)
STRONG_ALIAS(_atomic_dec_ptr_nv,_atomic_dec_32_nv)
