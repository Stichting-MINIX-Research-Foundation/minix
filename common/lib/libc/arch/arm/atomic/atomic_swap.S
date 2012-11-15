/*	$NetBSD: atomic_swap.S,v 1.4 2012/08/31 23:41:52 matt Exp $	*/

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

/*
 * While SWP{B} is sufficient on its own for pre-ARMv7 CPUs, on MP ARMv7 cores
 * SWP{B} is disabled since it's no longer atomic among multiple CPUs.  They
 * will actually raise an UNDEFINED exception.
 *
 * So if we use the LDREX/STREX template, but use a SWP instruction followed
 * by a MOV instruction (using a temporary register), that gives a handler
 * for the SWP UNDEFINED exception enough information to "patch" this instance
 * SWP with correct forms of LDREX/STREX.  (note that this would happen even
 * "read-only" pages.  If the page gets tossed, we will get another exception
 * and fix yet again).
 */

ENTRY_NP(_atomic_swap_32)
	mov	r2, r0
1:
#ifdef _ARM_ARCH_6
	ldrex	r0, [r2]
	cmp	r0, r1
	strexne	ip, r1, [r2]
#else
	swp	r0, r1, [r2]
	cmp	r0, r1
	movsne	ip, #0
#endif
	cmpne	ip, #0
	bne	1b
#ifdef _ARM_ARCH_7
	dmb
#else
	mcr	p15, 0, ip, c7, c10, 5	/* data memory barrier */
#endif
	RET
	END(_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_32,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_uint,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_ulong,_atomic_swap_32)
ATOMIC_OP_ALIAS(atomic_swap_ptr,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_uint,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ulong,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ptr,_atomic_swap_32)

ENTRY_NP(_atomic_swap_8)
	mov	r2, r0
1:
#ifdef _ARM_ARCH_6
	ldrexb	r0, [r2]
	strexb	r3, r1, [r2]
#else
	swpb	r0, r1, [r2]
	mov	r3, #0
#endif
	cmp	r3, #0
	bne	1b
#ifdef _ARM_ARCH_7
	dmb
#else
	mcr	p15, 0, ip, c7, c10, 5	/* data memory barrier */
#endif
	RET
	END(_atomic_swap_8)
ATOMIC_OP_ALIAS(atomic_swap_8,_atomic_swap_8)
ATOMIC_OP_ALIAS(atomic_swap_char,_atomic_swap_8)
ATOMIC_OP_ALIAS(atomic_swap_uchar,_atomic_swap_8)
STRONG_ALIAS(_atomic_swap_char,_atomic_swap_8)
STRONG_ALIAS(_atomic_swap_uchar,_atomic_swap_8)
