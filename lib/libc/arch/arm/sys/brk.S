/*	$NetBSD: brk.S,v 1.13 2013/09/12 15:36:15 joerg Exp $	*/

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
 *	from: @(#)brk.s	5.2 (Berkeley) 12/17/90
 */

#include "SYS.h"

	.globl	_C_LABEL(_end)
	.globl	_C_LABEL(__curbrk)

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.data
	.align	0
	.globl	_C_LABEL(__minbrk)
	.hidden	_C_LABEL(__minbrk)
	.type	_C_LABEL(__minbrk),#object
_C_LABEL(__minbrk):
	.word	_C_LABEL(_end)

/*
 * Change the data segment size
 */
ENTRY(_brk)
	ldr	r1, .Lminbrk
#ifdef __PIC__
	adr	r3, .Lminbrk
	adds	r1, r1, r3
#endif
	/* Get the minimum allowable brk address */
	ldr	r1, [r1]

	/*
	 * Valid the address specified and set to the minimum
	 * if the address is below minbrk.
	 */
	cmp	r0, r1
#ifdef __thumb__
	bcs	1f
	mov	r0, r1
1:
#else
	movcc	r0, r1
#endif
	mov	r2, r0
	SYSTRAP(break)
	_INVOKE_CERROR()

	ldr	r1, .Lcurbrk
#ifdef __PIC__
	adds	r1, r1, r3
#endif
	/* Store the new address in curbrk */
	str	r2, [r1]

	/* Return 0 for success */
	movs	r0, #0
	RET

	.align	0
.Lminbrk:
	.word	REL_SYM(_C_LABEL(__minbrk), .Lminbrk)
.Lcurbrk:
	.word	REL_SYM(_C_LABEL(__curbrk), .Lminbrk)
END(_brk)
