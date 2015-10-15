/* $NetBSD: _setjmp.S,v 1.1 2014/08/10 05:47:36 matt Exp $ */

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
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a, v)
 * will generate a "return v;" from the last call to
 *	_setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is NOT restored.
 *
 * Note: x0 is the return value
 */
	.section	.rodata.cst8,"aM",@progbits,8
	.align	3
.L_MAGIC:
	.xword	_JB_MAGIC_AARCH64__SETJMP

ENTRY(_setjmp)
	adrp	x7, .L_MAGIC
	ldr	x7, [x7, #:lo12:.L_MAGIC]

	mov	x3, sp

	stp	x7, x3, [x0, #_JB_MAGIC]

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

        mov	x0, xzr
        ret
END(_setjmp)

ENTRY(_longjmp)
	adrp	x7, .L_MAGIC
	ldr	x7, [x7, #:lo12:.L_MAGIC]

	ldp	x2, x3, [x0, #_JB_MAGIC]
	ldp	x4, x5, [x0, #_JB_X29]

	cbz	x3, .Lbotch
	cbz	x4, .Lbotch
	cbz	x5, .Lbotch
	cmp	x2, x7
	b.ne	.Lbotch

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

	mov	sp, x3
	mov	x29, x4
	mov	x30, x5

        mov	x0, x1
        ret

	/* validation failed, die die die. */
.Lbotch:
	bl	_C_LABEL(longjmperror)
	bl	_C_LABEL(abort)
1:	b	1b		/* Cannot get here */
END(_longjmp)
