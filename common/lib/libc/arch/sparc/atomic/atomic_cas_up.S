/*	$NetBSD: atomic_cas_up.S,v 1.2 2014/01/29 13:06:36 martin Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include <sys/ras.h>
#include "atomic_op_asm.h"

	.text

ENTRY(_atomic_cas_up)
	.hidden	_C_LABEL(_atomic_cas_up)
RAS_START_ASM_HIDDEN(_atomic_cas)
	ld	[%o0], %o3			! get old value
	cmp	%o1, %o3			! old == new?
	beq,a	1f				! yes, do the store
	 st	%o2, [%o0]			! (in the delay slot)
RAS_END_ASM_HIDDEN(_atomic_cas)
1:
	retl
	 mov	%o3, %o0			! return old value

ENTRY(_atomic_cas_16_up)
	.hidden	_C_LABEL(_atomic_cas_16_up)
RAS_START_ASM_HIDDEN(_atomic_cas_16)
	lduh	[%o0], %o3			! get old value
	cmp	%o1, %o3			! old == new?
	beq,a	1f				! yes, do the store
	 sth	%o2, [%o0]			! (in the delay slot)
RAS_END_ASM_HIDDEN(_atomic_cas_16)
1:
	retl
	 mov	%o3, %o0			! return old value

ENTRY(_atomic_cas_8_up)
	.hidden	_C_LABEL(_atomic_cas_8_up)
RAS_START_ASM_HIDDEN(_atomic_cas_8)
	ldub	[%o0], %o3			! get old value
	cmp	%o1, %o3			! old == new?
	beq,a	1f				! yes, do the store
	 stb	%o2, [%o0]			! (in the delay slot)
RAS_END_ASM_HIDDEN(_atomic_cas_8)
1:
	retl
	 mov	%o3, %o0			! return old value

