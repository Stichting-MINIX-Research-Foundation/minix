/*	$NetBSD: setjmp.S,v 1.9 2011/04/30 19:39:38 martin Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 * from: Header: setjmp.s,v 1.2 92/06/25 03:18:43 torek Exp
 */

#define _LOCORE
#include <machine/asm.h>
#include <machine/frame.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	.asciz "@(#)setjmp.s	8.1 (Berkeley) 6/4/93"
#else
	RCSID("$NetBSD: setjmp.S,v 1.9 2011/04/30 19:39:38 martin Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- setjmp
 *
 *	__longjmp14(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and a struct sigcontext, see <signal.h>
 */

#include "SYS.h"

	.register %g2,#ignore
	.register %g3,#ignore
	.register %g6,#ignore

ENTRY(__setjmp14)
	save	%sp, -CC64FSZ, %sp

	/* save globals into locals */
	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7

#ifdef PIC
	PIC_PROLOGUE(%i1, %o4)
#endif

	/* now get sigmask and onstack flag */
	add	%i0, 0x38, %o2		/* build sigcontext in [%o2]->sc.sc_mask */
	mov	1, %o0			/* SIG_BLOCK */
					/* sigprocmask(SIG_BLOCK, (sigset_t *)NULL, (sigset_t *)a) */
#ifdef PIC
#ifdef BIGPIC
	set	_C_LABEL(__sigprocmask14), %o4
	ldx	[%i1 + %o4], %g2
#else
	ldx	[%i1 + _C_LABEL(__sigprocmask14)], %g2
#endif
	call	%g2
#else
	call	_C_LABEL(__sigprocmask14)
#endif
	 clr	%o1

	clr	%o0			/* sigstack(NULL, &foo) */
#ifdef PIC
#ifdef BIGPIC
	set	_C_LABEL(__sigaltstack14), %o4
	ldx	[%i1 + %o4], %g2
#else
	ldx	[%i1 + _C_LABEL(__sigaltstack14)], %g2
#endif
	call	%g2
#else
	call	_C_LABEL(__sigaltstack14)
#endif
	 add	%i0, 8, %o1		/* (foo being part of the sigcontext we're overwriting) */
	
	lduw	[%i0 + 8 + 0x10], %o0	/* foo.ss_flags */
	and	%o0, 1, %o1		/* onstack = foo.ss_flags & SS_ONSTACK; */
	st	%o1, [%i0 + 0x00]	/* sc.sc_onstack = current onstack; */

	/* store essential state */
	stx	%fp, [%i0 + 0x08]	/* sc.sc_sp = sp */
	add	%i7, 8, %o0
	stx	%o0, [%i0 + 0x10]	/* sc.sc_pc = return_pc */
	stx	%g0, [%i0 + 0x20]	/* sc.sc_tstate = (clean ccr) */
	stx	%l1, [%i0 + 0x28]	/* sc.sc_g1 */
	stx	%l2, [%i0 + 0x30]	/* sc.sc_o0, set in longjmp, use as %g2 */
	/* 0x38: 4x32bit */		/* sc.sc_mask was already saved above */

	/* save additional registers needed to fill a complete mcontext */
	stx	%l3, [%i0 + 0x48]	/* adjust asserts in longjmp.c */
	stx	%l6, [%i0 + 0x50]	/* if you change any of these */
	stx	%l7, [%i0 + 0x58]	/* offsets! */

	ret				/* return 0 */
	 restore %g0, 0, %o0
