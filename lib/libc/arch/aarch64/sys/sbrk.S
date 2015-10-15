/* $NetBSD: sbrk.S,v 1.1 2014/08/10 05:47:37 matt Exp $ */

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

WEAK_ALIAS(sbrk, _sbrk)

	.data
	.align	3
	.globl	_C_LABEL(__curbrk)
	.hidden	_C_LABEL(__curbrk)
	.type	_C_LABEL(__curbrk),#object
_C_LABEL(__curbrk):
	.xword	_C_LABEL(_end)
	.size	_C_LABEL(_end), 8

/*
 * Change the data segment size
 */
ENTRY(_sbrk)
	/* get address or offset to __curbrk */
	adrp	x9, _C_LABEL(__curbrk)

	/* Get the current brk address */
	ldr	x1, [x9, #:lo12:_C_LABEL(__curbrk)]

	/* Calculate new value */
	mov	x10, x0
	add	x0, x0, x1
	SYSTRAP(break)
	_INVOKE_CERROR()

	/* Store new curbrk value */
	ldr	x0, [x9, #:lo12:_C_LABEL(__curbrk)]
	add	x1, x0, x10
	str	x1, [x9, #:lo12:_C_LABEL(__curbrk)]

	/* Return old curbrk value */
	ret
END(_sbrk)
