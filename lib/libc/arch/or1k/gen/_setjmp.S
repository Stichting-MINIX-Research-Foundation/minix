/*	$NetBSD: _setjmp.S,v 1.1 2014/09/03 19:34:25 matt Exp $	*/

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

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: _setjmp.S,v 1.1 2014/09/03 19:34:25 matt Exp $")
#endif

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v?v:1)" from the last call to
 *	_setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is NOT restored.
 */

ENTRY(_setjmp)
	l.sw	JB_MAGIC(r3), r0	/* indicate no sigmask */
	l.sw	JB_PC(r3), lr		/* save return address */
	l.sw	JB_LR(r3), lr		/* save return address */
	l.sw	JB_SP(r3), r1		/* save stack pointer */
	l.sw	JB_FP(r3), r2		/* save frame pointer */
	l.sw	JB_R10(r3), r10		/* save callee saved register */
	l.sw	JB_R14(r3), r14		/* save callee saved register */
	l.sw	JB_R16(r3), r16		/* save callee saved register */
	l.sw	JB_R18(r3), r18		/* save callee saved register */
	l.sw	JB_R20(r3), r20		/* save callee saved register */
	l.sw	JB_R22(r3), r22		/* save callee saved register */
	l.sw	JB_R24(r3), r24		/* save callee saved register */
	l.sw	JB_R26(r3), r26		/* save callee saved register */
	l.sw	JB_R28(r3), r28		/* save callee saved register */
	l.sw	JB_R30(r3), r30		/* save callee saved register */
	l.xor	r11, r11, r11		/* indicate success */
	l.jr	lr			/* return */
	l.nop
END(_setjmp)

ENTRY(_longjmp)
	l.lwz	lr, JB_LR(r3)		/* get return address */
	l.lwz	r1, JB_SP(r3)		/* get stack pointer */
	l.lwz	r2, JB_FP(r3)		/* get frame pointer */
	l.lwz	r10, JB_R10(r3)		/* get callee saved register */
	l.lwz	r14, JB_R14(r3)		/* get callee saved register */
	l.lwz	r16, JB_R16(r3)		/* get callee saved register */
	l.lwz	r18, JB_R18(r3)		/* get callee saved register */
	l.lwz	r20, JB_R20(r3)		/* get callee saved register */
	l.lwz	r22, JB_R22(r3)		/* get callee saved register */
	l.lwz	r24, JB_R24(r3)		/* get callee saved register */
	l.lwz	r26, JB_R26(r3)		/* get callee saved register */
	l.lwz	r28, JB_R28(r3)		/* get callee saved register */
	l.lwz	r30, JB_R30(r3)		/* get callee saved register */
	l.ori	r11, r0, 1		/* default return value */
	l.sfeqi	r4, 0			/* test return value */
	l.cmov	r11, r11, r4		/* use it if not 0 */
	l.jr	lr			/* return */
	l.nop
END(_longjmp)
