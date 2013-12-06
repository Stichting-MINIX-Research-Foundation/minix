/*	$NetBSD: ptrace.S,v 1.12 2013/11/30 20:20:42 joerg Exp $ */

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
 *	from: @(#)ptrace.s	5.1 (Berkeley) 4/23/90
 */

#include "SYS.h"

ENTRY(ptrace)
#ifdef _REENTRANT
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	push	{r0-r3, lr}
#else
	push	{r0-r4}
	mov	r4, lr
#endif
	sub	sp, sp, #4	/* align stack */
	bl	PLT_SYM(_C_LABEL(__errno))
	add	sp, sp, #4	/* unalign stack */
	movs	r1, #0
	str	r1, [r0]
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	pop	{r0-r3, lr}
#else
	mov	lr, r4
	pop	{r0-r4}
#endif
#else
	push	{r0, r1}
#ifdef __PIC__
	/* Setup the GOT */
	ldr	r0, .Lgot
	adr	r1, .Lgot
	add	r0, r0, r1
	ldr	r1, .Lerrno
	ldr	r1, [r0, r1]
#else
	ldr	r1, .Lerrno
#endif /* __PIC__ */
	movs	r0, #0
	str	r0, [r1]
	pop	{r0, r1}
#endif /* _REENTRANT */

	SYSTRAP(ptrace)
	_INVOKE_CERROR()
	RET

#ifndef _REENTRANT
#ifdef __PIC__
	.align	0
.Lgot:
	.word	_C_LABEL(_GLOBAL_OFFSET_TABLE_) - .Lgot
#endif /* __PIC__ */

.Lerrno:
	.word	PIC_SYM(_C_LABEL(errno), GOT)
#endif /* !_REENTRANT */
END(ptrace)
