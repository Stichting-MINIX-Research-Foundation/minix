/*	$NetBSD: sbrk.S,v 1.12 2013/09/12 15:36:15 joerg Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)sbrk.s	5.1 (Berkeley) 4/23/90
 */

#include "SYS.h"

	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

	.data
	.align	0
	.globl	_C_LABEL(__curbrk)
	.hidden	_C_LABEL(__curbrk)
	.type	_C_LABEL(__curbrk),#object
_C_LABEL(__curbrk):
	.word	_C_LABEL(_end)

/*
 * Change the data segment size
 */
ENTRY(_sbrk)
	/* get address or offset to __curbrk */
	ldr	r2, .Lcurbrk
#ifdef __PIC__
	add	r2, r2, pc	/* pc = &.LPIC0 */
#endif

	/* Get the current brk address */
	ldr	r1, [r2]

	/* Calculate new value */
.LPIC0:	mov	r3, r0
	adds	r0, r0, r1
	SYSTRAP(break)
	_INVOKE_CERROR()

	/* Store new curbrk value */
	ldr	r0, [r2]
	adds	r1, r0, r3
	str	r1, [r2]

	/* Return old curbrk value */
	RET

	.align	0
.Lcurbrk:
	.word	REL_SYM(_C_LABEL(__curbrk), .LPIC0)
END(_sbrk)
