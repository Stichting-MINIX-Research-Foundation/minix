/*	$NetBSD: atomic_and.S,v 1.2 2009/12/14 00:38:59 matt Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#include <sys/cdefs.h>
#include <machine/asm.h>
#include "atomic_op_asm.h"

RCSID("$NetBSD: atomic_and.S,v 1.2 2009/12/14 00:38:59 matt Exp $")

	.text
	.set	noat
	.set	noreorder
	.set	nomacro

LEAF(_atomic_and_32)
1:	INT_LL		t0, 0(a0)
	 nop
	and		t0, a1
	INT_SC		t0, 0(a0)
	beq		t0, zero, 1b
 	 nop
	j		ra
	 nop
END(_atomic_and_32)
ATOMIC_OP_ALIAS(atomic_and_32, _atomic_and_32)

LEAF(_atomic_and_32_nv)
1:	INT_LL		v0, 0(a0)
	 nop
	and		v0, a1
	move		t0, v0
	INT_SC		t0, 0(a0)
	beq		t0, zero, 1b
 	 nop
	j		ra
	 nop
END(_atomic_and_32_nv)
ATOMIC_OP_ALIAS(atomic_and_32_nv, _atomic_and_32_nv)

#if !defined(__mips_o32)
LEAF(_atomic_and_64)
1:	REG_LL		t0, 0(a0)
	 nop
	and		t0, a1
	REG_SC		t0, 0(a0)
	beq		t0, zero, 1b
 	 nop
	j		ra
	 nop
END(_atomic_and_64)
ATOMIC_OP_ALIAS(atomic_and_64, _atomic_and_64)

LEAF(_atomic_and_64_nv)
1:	REG_LL		v0, 0(a0)
	 nop
	and		v0, a1
	move		t0, v0
	REG_SC		t0, 0(a0)
	beq		t0, zero, 1b
 	 nop
	j		ra
	 nop
END(_atomic_and_64_nv)
ATOMIC_OP_ALIAS(atomic_and_64_nv, _atomic_and_64_nv)
#endif

#ifdef _LP64
STRONG_ALIAS(_atomic_and_ulong,		_atomic_and_64)
STRONG_ALIAS(_atomic_and_ulong_nv,	_atomic_and_64_nv)
#else
STRONG_ALIAS(_atomic_and_ulong,		_atomic_and_32)
STRONG_ALIAS(_atomic_and_ulong_nv,	_atomic_and_32_nv)
#endif
STRONG_ALIAS(_atomic_and_uint,		_atomic_and_32)
STRONG_ALIAS(_atomic_and_uint_nv,	_atomic_and_32_nv)

ATOMIC_OP_ALIAS(atomic_and_uint,	_atomic_and_uint)
ATOMIC_OP_ALIAS(atomic_and_uint_nv,	_atomic_and_uint_nv)
ATOMIC_OP_ALIAS(atomic_and_ulong,	_atomic_and_ulong)
ATOMIC_OP_ALIAS(atomic_and_ulong_nv,	_atomic_and_ulong_nv)
