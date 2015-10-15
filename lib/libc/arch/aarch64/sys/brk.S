/* $NetBSD: brk.S,v 1.1 2014/08/10 05:47:37 matt Exp $ */

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

#include "SYS.h"

	.globl	_C_LABEL(_end)
	.globl	_C_LABEL(__curbrk)

WEAK_ALIAS(brk, _brk)

	.data
	.globl	_C_LABEL(__minbrk)
	.hidden	_C_LABEL(__minbrk)
	.type	_C_LABEL(__minbrk),#object
	.align	3
_C_LABEL(__minbrk):
	.xword	_C_LABEL(_end)
	.size	_C_LABEL(__minbrk), 8

/*
 * Change the data segment size
 */
ENTRY(_brk)
	adr	x9, _C_LABEL(__minbrk)
	ldr	x10, [x9, #:lo12:_C_LABEL(__minbrk)]

	/*
	 * Valid the address specified and set to the minimum
	 * if the address is below minbrk.
	 */
	cmp	x0, x10
	csel	x0, x10, x0, lt
	mov	x11, x0
	SYSTRAP(break)
	_INVOKE_CERROR()

	/* Store the new address in curbrk */
	adr	x9, _C_LABEL(__curbrk)
	str	x11, [x9, #:lo12:_C_LABEL(__curbrk)]

	/* Return 0 for success */
	mov	x0, xzr
	ret
END(_brk)
