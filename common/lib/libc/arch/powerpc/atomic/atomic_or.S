/*	$NetBSD: atomic_or.S,v 1.6 2011/01/15 07:31:11 matt Exp $	*/

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

__RCSID("$NetBSD: atomic_or.S,v 1.6 2011/01/15 07:31:11 matt Exp $")

	.text
ENTRY(_atomic_or_32)
1:	lwarx	%r10,0,%r3
	or	%r10,%r10,%r4
	stwcx.	%r10,0,%r3
	bne-	1b
	blr
END(_atomic_or_32)
ATOMIC_OP_ALIAS(atomic_or_32,_atomic_or_32)
ATOMIC_OP_ALIAS(atomic_or_uint,_atomic_or_32)
STRONG_ALIAS(_atomic_or_uint,_atomic_or_32)
#if !defined(_LP64)
ATOMIC_OP_ALIAS(atomic_or_ulong,_atomic_or_32)
STRONG_ALIAS(_atomic_or_ulong,_atomic_or_32)
#endif

ENTRY(_atomic_or_32_nv)
1:	lwarx	%r10,0,%r3
	or	%r10,%r10,%r4
	stwcx.	%r10,0,%r3
	bne-	1b
	mr	%r3,%r10
	blr
END(_atomic_or_32_nv)
ATOMIC_OP_ALIAS(atomic_or_32_nv,_atomic_or_32_nv)
ATOMIC_OP_ALIAS(atomic_or_uint_nv,_atomic_or_32_nv)
STRONG_ALIAS(_atomic_or_uint_nv,_atomic_or_32_nv)
#if !defined(_LP64)
ATOMIC_OP_ALIAS(atomic_or_ulong_nv,_atomic_or_32_nv)
STRONG_ALIAS(_atomic_or_ulong_nv,_atomic_or_32_nv)
#endif
