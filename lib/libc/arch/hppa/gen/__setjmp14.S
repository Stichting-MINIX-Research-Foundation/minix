/*	$NetBSD: __setjmp14.S,v 1.5 2008/04/28 20:22:55 martin Exp $	*/

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
#include <machine/psl.h>

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: __setjmp14.S,v 1.5 2008/04/28 20:22:55 martin Exp $")
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

ENTRY(__setjmp14,0)
	/* Finish our stack frame. */
	stw	%rp, HPPA_FRAME_CRP(%sp)
	stw	%arg0, HPPA_FRAME_ARG(0)(%sp)

	/* A sigcontext is at the beginning of our jmp_buf. */
	stw	%r0, 4(%arg0)		; unused word (old style signal mask)
	stw	%sp, 8(%arg0)		; sc.sc_sp = %sp
	stw	%r0, 16(%arg0)		; sc.sc_ap = NULL
	mfsp	%sr0, %r1
	stw	%r1, 20(%arg0)		; sc.sc_pcsqh = %sr0
	stw	%rp, 24(%arg0)		; sc.sc_pcoqh = %rp
	stw	%r1, 28(%arg0)		; sc.sc_pcsqh = %sr0
	ldo	4(%rp), %r1
	stw	%r1, 32(%arg0)		; sc.sc_pcoqt = %rp + 4
#define PSW_MBS (PSW_C|PSW_Q|PSW_P|PSW_D|PSW_I)
	ldil	L%PSW_MBS, %r1
	ldo	R%PSW_MBS(%r1), %r1
	stw	%r1, 36(%arg0)		; set sc.sc_ps

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

	/*
	 * Start the stack frame for the calls we will make.
	 * The minimum frame is 48 bytes, but the stack must
	 * always be 64-byte aligned; we use the top 4 bytes
	 * to save our caller's %r3, and the following 12 bytes
	 * as the stack_t for our sigaltstack call.
	 * XXX - this assumes that sizeof(stack_t) <= 12
	 */
	copy	%r3, %r1
	copy	%sp, %r3
	stwm	%r1, HPPA_FRAME_SIZE(%sp)

	/* Get signal stack info. */
	ldo	4(%r3), %arg1		; set up oss
	bl	__sigaltstack14, %rp
	copy	%r0, %arg0		; set up ss

	/* Recover our jmp_buf and extract SS_ONSTACK */
	ldw	HPPA_FRAME_ARG(0)(%r3), %arg0
	ldw	12(%r3), %r1		; get ss_flags
	ldi	1, %r20			; SS_ONSTACK
	and	%r1, %r20, %r1
	stw	%r1, 0(%arg0)		; sc.sc_onstack
	
	/* Get the signal mask. */
	ldo	40(%arg0), %arg2	; oset = &sc.sc_mask	
	copy	%r0, %arg1		; set = NULL
	bl	__sigprocmask14, %rp
	copy	%r0, %arg0		; action = 0 <ignored>

	/* Return 0. */
	ldo	HPPA_FRAME_SIZE(%r3),%sp
	ldwm	-HPPA_FRAME_SIZE(%sp),%r3
	ldw	HPPA_FRAME_CRP(%sp), %rp
	bv	%r0(%rp)
	copy	%r0, %ret0
EXIT(__setjmp14)

	.end
