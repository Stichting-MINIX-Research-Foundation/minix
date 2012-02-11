/*	$NetBSD: _setjmp.S,v 1.4 2008/04/28 20:22:56 martin Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthew Fredette.
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
/* XXX fredette - this is awful */
#define _LOCORE
#include <machine/frame.h>
#undef _LOCORE

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: _setjmp.S,v 1.4 2008/04/28 20:22:56 martin Exp $")
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and a struct sigcontext, see <signal.h>
 */

ENTRY(_setjmp,0)
	/* A sigcontext is at the beginning of our jmp_buf. */
	stw	%r0, 0(%arg0)		; no onstack
	stw	%r0, 4(%arg0)		; unused word (old style signal mask)
	stw	%sp, 8(%arg0)		; sc.sc_sp = %sp
	stw	%r0, 16(%arg0)		; sc.sc_ap = NULL
	mfsp	%sr0, %r1
	stw	%r1, 20(%arg0)		; sc.sc_pcsqh = %sr0
	stw	%rp, 24(%arg0)		; sc.sc_pcoqh = %rp
	stw	%r1, 28(%arg0)		; sc.sc_pcsqh = %sr0
	ldo	4(%rp), %r1
	stw	%r1, 32(%arg0)		; sc.sc_pcoqt = %rp + 4
	stw	%r0, 36(%arg0)		; set sc.sc_ps

	/* We store all callee-saved registers after the sigcontext. */
	ldo	56(%arg0), %r1
	stwm	%r3, 4(%r1)
	stwm	%r4, 4(%r1)
	stwm	%r5, 4(%r1)
	stwm	%r6, 4(%r1)
	stwm	%r7, 4(%r1)
	stwm	%r8, 4(%r1)
	stwm	%r9, 4(%r1)
	stwm	%r10, 4(%r1)
	stwm	%r11, 4(%r1)
	stwm	%r12, 4(%r1)
	stwm	%r13, 4(%r1)
	stwm	%r14, 4(%r1)
	stwm	%r15, 4(%r1)
	stwm	%r16, 4(%r1)
	stwm	%r17, 4(%r1)
	stwm	%r18, 4(%r1)

	/* Return 0. */
	bv	%r0(%rp)
	copy	%r0, %ret0
EXIT(_setjmp)

ENTRY(_longjmp,0)
	ldw	8(%arg0), %r1		; ensure non-zero SP
	combt,=,n	%r0, %r1, botch	; oops!
	add,<>	%r0, %arg1, %ret0	; ensure return value non-zero
	ldi	1, %ret0

	/* restore callee-saved registers */
	ldo	56(%arg0), %r1
	ldwm	4(%r1), %r3
	ldwm	4(%r1), %r4
	ldwm	4(%r1), %r5
	ldwm	4(%r1), %r6
	ldwm	4(%r1), %r7
	ldwm	4(%r1), %r8
	ldwm	4(%r1), %r9
	ldwm	4(%r1), %r10
	ldwm	4(%r1), %r11
	ldwm	4(%r1), %r12
	ldwm	4(%r1), %r13
	ldwm	4(%r1), %r14
	ldwm	4(%r1), %r15
	ldwm	4(%r1), %r16
	ldwm	4(%r1), %r17
	ldwm	4(%r1), %r18

	/* restore the rest */
	ldw	8(%arg0), %sp
	ldw	24(%arg0), %rp
	bv	%r0(%rp)
	nop
botch:
	bl	longjmperror, %rp
	nop
	bl	abort, %rp
	nop
EXIT(_longjmp)

	.end
