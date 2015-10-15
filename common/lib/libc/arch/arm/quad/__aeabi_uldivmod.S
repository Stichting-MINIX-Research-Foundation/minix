/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

RCSID("$NetBSD: __aeabi_uldivmod.S,v 1.9 2014/05/06 16:02:11 joerg Exp $")

/*
 * typedef struct { unsigned long long quo, rem } ulldiv_t;
 * __value_in_regs ulldiv_t __aeabi_uldivmod(unsigned long long n,
 *	unsigned long long d);
 */

ENTRY(__aeabi_uldivmod)
#ifdef __ARM_EABI__
# if !defined(__ARM_DWARF_EH__)
	.fnstart
#endif
	.cfi_startproc
#endif
#if !defined(_KERNEL) && !defined(_STANDALONE)
#if !defined(__thumb__)
	orrs	ip, r2, r3
	beq	.Ldivbyzero
#elif defined(_ARM_ARCH_T2)
	cbnz	r2, 1f
	cbz	r3, .Ldivbyzero
1:
#else
	cmp	r2, #0
	bne	1f
	cmp	r3, #0
	beq	.Ldivbyzero
1:
#endif
#endif

	push	{r4,lr}
#ifdef __ARM_EABI__
# if !defined(__ARM_DWARF_EH__)
	.save	{r4,lr}
# endif
	.cfi_def_cfa_offset 8
	.cfi_offset 14, -4
	.cfi_offset 4, -8
#endif
	sub	sp, sp, #16
#ifdef __ARM_EABI__
	.cfi_def_cfa_offset 24
#endif
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	add	r4, sp, #8
#else
	mov	r4, sp
	adds	r4, r4, #8
#endif
	str	r4, [sp]
	bl	PLT_SYM(__qdivrem)
	add	sp, sp, #8
#ifdef __ARM_EABI__
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -12
	.cfi_offset 2, -16
#endif
	/*
	 * The remainder is already on the stack just waiting to be popped
	 * into r2/r3.
	 */
	pop	{r2-r4,pc}

#if !defined(_KERNEL) && !defined(_STANDALONE)
.Ldivbyzero:
	push	{r0-r1,r4,lr}
#ifdef __ARM_EABI__
# if !defined(__ARM_DWARF_EH__)
	.save	{r0-r1,r4,lr}
# endif
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -4
	.cfi_offset 4, -8
#endif
#ifdef __thumb__
	movs	r0, #0
	mvns	r0, r0
#else
	mvn	r0, #0
#endif
	movs	r1, r0
	bl	PLT_SYM(__aeabi_ldiv0)
	pop	{r2-r4,pc}
#endif
#ifdef __ARM_EABI__
	.cfi_endproc
# if !defined(__ARM_DWARF_EH__)
	.fnend
# endif
#endif
END(__aeabi_uldivmod)
