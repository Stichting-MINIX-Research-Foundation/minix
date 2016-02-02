/*	$NetBSD: atomic_cas_up.S,v 1.4 2014/01/29 11:34:25 skrll Exp $	*/

/*-
 * Copyright (c) 2008, 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nick Hudson
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

#include <sys/ras.h>
#include <machine/asm.h>

LEAF_ENTRY_NOPROFILE(_atomic_cas_up)
	.hidden	_C_LABEL(_atomic_cas_up)

RAS_START_ASM_HIDDEN(_atomic_cas)
	ldw	0(%arg0), %ret0
	comb,<>,n	%arg1, %ret0, 1f
	stw	%arg2, 0(%arg0)
RAS_END_ASM_HIDDEN(_atomic_cas)
1:
	bv,n	%r0(%rp)

EXIT(_atomic_cas_up)

LEAF_ENTRY_NOPROFILE(_atomic_cas_16_up)
	.hidden	_C_LABEL(_atomic_cas_16_up)

RAS_START_ASM_HIDDEN(_atomic_cas_16)
	ldh	0(%arg0),%ret0
	comb,<>,n	%arg1, %ret0, 1f
	sth	%arg2, 0(%arg0)
RAS_END_ASM_HIDDEN(_atomic_cas_16)
1:
	bv,n	%r0(%rp)
EXIT(_atomic_cas_16_up)

LEAF_ENTRY_NOPROFILE(_atomic_cas_8_up)
	.hidden	_C_LABEL(_atomic_cas_8_up)

RAS_START_ASM_HIDDEN(_atomic_cas_8)
	ldb	0(%arg0),%ret0
	comb,<>,n	%arg1, %ret0, 1f
	stb	%arg2, 0(%arg0)
RAS_END_ASM_HIDDEN(_atomic_cas_8)
1:
	bv,n	%r0(%rp)
EXIT(_atomic_cas_8_up)
