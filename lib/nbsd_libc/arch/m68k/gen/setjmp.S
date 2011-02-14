/*	$NetBSD: setjmp.S,v 1.12 2006/03/09 16:20:27 christos Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)setjmp.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: setjmp.S,v 1.12 2006/03/09 16:20:27 christos Exp $")
#endif
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

ENTRY(__setjmp14)
	/* Get signal stack info.  Note overlay of ss_sp and ss_size! */
	lea	%sp@(-12),%sp	/* sizeof(stack_t) */
	clrl	%sp@		/* ss = NULL */
	movl	%sp,%sp@(4)	/* oss = stack_t on stack */
	jbsr	PIC_PLT(_C_LABEL(__sigaltstack14))

	movl	%sp@(8),%d0	/* ss_flags */
	andl	#1,%d0		/* extract SS_ONSTACK */
	lea	%sp@(12),%sp	/* pop stack_t */

	/* Get pointer to jmp_buf; a sigcontext is at the beginning. */
	movl	%sp@(4),%a0
	movl	%d0,%a0@	/* store onstack */
	clrl	%a0@(4)		/* unused word (old style signal mask) */

	/* Get the signal mask. */
	pea	%a0@(28)	/* oset = &sc.sc_mask */
	movl	#0,%sp@-	/* set = NULL */
	movl	#0,%sp@-	/* action = 0 <ignored> */
	jbsr	PIC_PLT(_C_LABEL(__sigprocmask14))
	addl	#12,%sp

	movl	%sp@(4),%a0	/* get jmp_buf pointer again */
	lea	%sp@(4),%a1	/* adjust SP since we won't rts */
	movl	%a1,%a0@(8)	/* save SP */
	movl	%a6,%a0@(12)	/* save FP */
	clrl	%a0@(16)	/* no AP */
	movl	%sp@,%a0@(20)	/* save return PC */
	clrl	%a0@(24)	/* clear PS */

	/* Save remaining non-scratch regs after signal mask. */
	moveml	#0x3CFC,%a0@(44)

	clrl	%d0		/* return 0 */
	rts

