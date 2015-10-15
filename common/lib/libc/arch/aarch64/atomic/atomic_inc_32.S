/* $NetBSD: atomic_inc_32.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

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
1:	ldxr	w3, [x0]		/* load old value (return value) */
	add	w3, w3, #1		/* calculate new value */
	stxr	w2, w3, [x0]		/* try to store */
	cbnz	w2, 1b			/*   succeed? no, try again? */
	dmb	st
	ret				/* return new value */
END(_atomic_inc_32)

ATOMIC_OP_ALIAS(atomic_inc_32,_atomic_inc_32)
ATOMIC_OP_ALIAS(atomic_inc_uint,_atomic_inc_32)
STRONG_ALIAS(_atomic_inc_uint,_atomic_inc_32)

ENTRY_NP(_atomic_inc_32_nv)
	mov	x4, x0			/* need r0 for return value */
1:	ldxr	w0, [x4]		/* load old value */
	add	w0, w0, #1		/* calculate new value (return value) */
	stxr	w3, w0, [x4]		/* try to store */
	cbnz	w3, 1b			/*   succeed? no, try again? */
	dmb	st
	ret				/* return new value */
END(_atomic_inc_32_nv)

ATOMIC_OP_ALIAS(atomic_inc_32_nv,_atomic_inc_32_nv)
ATOMIC_OP_ALIAS(atomic_inc_uint_nv,_atomic_inc_32_nv)
STRONG_ALIAS(_atomic_inc_uint_nv,_atomic_inc_32_nv)
