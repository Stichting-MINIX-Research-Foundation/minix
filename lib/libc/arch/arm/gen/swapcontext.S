/*	$NetBSD: swapcontext.S,v 1.14 2013/11/30 20:20:42 joerg Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
        RCSID("$NetBSD: swapcontext.S,v 1.14 2013/11/30 20:20:42 joerg Exp $")
#endif /* LIBC_SCCS && !lint */

ENTRY(swapcontext)
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
	.fnstart
	.cfi_startproc
#endif
	push	{r0-r2, lr}	/* Must save oucp, ucp, lr. */
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
	.save	{r0-r2, lr}
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -4
	.cfi_offset 2, -8
	.cfi_offset 1, -12
	.cfi_offset 0, -16
#endif
	bl	PLT_SYM(_C_LABEL(_getcontext))  /* getcontext(oucp) */
	cmp	r0, #0
#if !defined(__thumb__)
	pop	{r0-r2, lr}
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
	.cfi_def_cfa_offset 0
#endif
	RETc(ne)
#else
	pop	{r0-r3}
	beq	1f
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
	.cfi_def_cfa_offset 0
	.cfi_register 14, 3
#endif
	RETr(r3)
1:
#endif
#if !defined(__thumb__)
	str	sp, [r0, #_UC_REGS_SP]	/* Adjust saved SP. */
	str	lr, [r0, #_UC_REGS_PC]	/* Adjust saved SP. */
#else
	mov	r2, sp
	str	r2, [r0, #_UC_REGS_SP]	/* Adjust saved SP. */
	str	r3, [r0, #_UC_REGS_PC]	/* Adjust saved PC. */
	mov	lr, r3
#endif
#ifdef SOFTFLOAT
	/* Ahem. */
#endif
	mov	r0, r1
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	b	PLT_SYM(_C_LABEL(setcontext))
#else
	push	{lr}
	bl	PLT_SYM(_C_LABEL(setcontext))
	pop	{pc}
#endif
#if defined(__ARM_EABI__) && defined(__UNWIND_TABLES__)
	.cfi_endproc
	.fnend
#endif
END(swapcontext)
