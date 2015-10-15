/*.$NetBSD: setjmp.S,v 1.1 2014/08/10 05:47:36 matt Exp $.*/

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

#include <machine/asm.h>
#include "assym.h"

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

	.section	.rodata.cst8,"aM",@progbits,8
	.align	3
.L_MAGIC:
	.xword	_JB_MAGIC_AARCH64_SETJMP

ENTRY(__setjmp14)
	adrp	x7, .L_MAGIC
	ldr	x7, [x7, #:lo12:.L_MAGIC]

	mov	x3, sp
	stp	x7,  x3,  [x0, #_JB_MAGIC]

	stp	x19, x20, [x0, #_JB_X19]
	stp	x21, x22, [x0, #_JB_X21]
	stp	x23, x24, [x0, #_JB_X23]
	stp	x25, x26, [x0, #_JB_X25]
	stp	x27, x28, [x0, #_JB_X27]
	stp	x29, x30, [x0, #_JB_X29]

	mrs	x5, tpidr_el0
	str	x5, [x0, #_JB_TPIDR]

	stp	d8,  d9,  [x0, #_JB_D8]
	stp	d10, d11, [x0, #_JB_D10]
	stp	d12, d13, [x0, #_JB_D12]
	stp	d14, d15, [x0, #_JB_D14]

	/* Get the signal mask. */
	add	x2, x0, #_JB_SIGMASK
	mov	x1, #0
	mov	x0, #0

	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	bl	_C_LABEL(__sigprocmask14)
	ldp	x29, x30, [sp], #16

	mov	x0, xzr
	ret
END(__setjmp14)

ENTRY(__longjmp14)
	adrp	x7, .L_MAGIC
	ldr	x7, [x7, #:lo12:.L_MAGIC]
	ldp	x2, x3, [x0, #_JB_MAGIC]
	cmp	x2, x7
	b.ne	.Lbotch

	ldp	x4,  x5,  [x0, #_JB_X29]
	cbz	x3, .Lbotch
	cbz	x4, .Lbotch
	cbz	x5, .Lbotch

	ldp	x19, x20, [x0, #_JB_X19]
	ldp	x21, x22, [x0, #_JB_X21]
	ldp	x23, x24, [x0, #_JB_X23]
	ldp	x25, x26, [x0, #_JB_X25]
	ldp	x27, x28, [x0, #_JB_X27]

	ldr	x5, [x0, #_JB_TPIDR]
	msr	tpidr_el0, x5 

	ldp	d8,  d9,  [x0, #_JB_D8]
	ldp	d10, d11, [x0, #_JB_D10]
	ldp	d12, d13, [x0, #_JB_D12]
	ldp	d14, d15, [x0, #_JB_D14]

	sub	sp, x3, #32

	stp	x4, x5, [sp, #16]
	str	x1, [sp, #8]
	add	x29, sp, #16

	mov	x2, #0
	add	x1, x0, #_JB_SIGMASK
	mov	x0, #3				/* SIG_SETMASK */
	bl	_C_LABEL(__sigprocmask14)

	ldp	x29, x30, [sp, #16]
	ldr	x0, [sp, #8]
	add	sp, sp, #32
	ret

	/* validation failed, die die die. */
.Lbotch:
	bl	_C_LABEL(longjmperror)
	bl	_C_LABEL(abort)
1:	b	1b		/* Cannot get here */
END(__longjmp14)
