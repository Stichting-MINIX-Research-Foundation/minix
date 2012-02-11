/*	$NetBSD: compat_setjmp.S,v 1.1 2005/10/16 04:41:34 christos Exp $	*/

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
	RCSID("$NetBSD: compat_setjmp.S,v 1.1 2005/10/16 04:41:34 christos Exp $")
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

#include "SYS.h"

#define STACK_T_SZ
ENTRY(setjmp)
	/*
	 * We use the part of the sigcontext structure, the sp, pc, and npc fields,
	 * for the sigstack call so we don't need to get our own stackframe.  It 
	 * won't be filled out till later anyway.
	 */
	mov	%o0, %o3		/* Save our jmp_buf in %o3 */
	mov	%o0, %o2		/* build sigcontext in [%o2] */
	mov	1, %o0			/* SIG_BLOCK */
	mov	SYS_compat_13_sigprocmask13, %g1
	clr	%o1			/* sigprocmask(SIG_BLOCK, (sigset_t *)NULL, (sigset_t *)a) */
	t	ST_SYSCALL
	
	st	%o0, [%o3 + 0x04]	/* sc.sc_mask = current mask; */
	mov	SYS___sigaltstack14, %g1
	clr	%o0			/* sigstack(NULL, &foo) */
	add	%o3, 0x38, %o1		/* (foo being the sigcontext14 sc_mask) */
	t	ST_SYSCALL
	
	lduw	[%o3 + 0x38+0x10], %o0	/* foo.ss_flags */
	and	%o0, 1, %o1		/* onstack = foo.ss_flags & 1; */
	st	%o0, [%o3 + 0x00]	/* sc.sc_onstack = current onstack; */
	stx	%sp, [%o3 + 0x08]	/* sc.sc_sp = sp (both ours and caller's) */
	add	%o7, 8, %o0
	stx	%o0, [%o3 + 0x10]	/* sc.sc_pc = return_pc */
	add	%o7, 12, %o0
	stx	%o0, [%o3 + 0x18]	/* sc.sc_npc = return_pc + 4 */
	stx	%g0, [%o3 + 0x20]	/* sc.sc_psr = (clean psr) */
	stx	%fp, [%o3 + 0x28]	/* sc.sc_g1 = %fp (misuse, but what the heck) */
					/* sc.sc_o0 = random(), set in longjmp */
	retl				/* return 0 */
	 clr	%o0

/*
 * All we need to do here is force sigreturn to load a new stack pointer,
 * new <pc,npc>, and appropriate %o0 return value from the sigcontext built
 * in setjmp.  The %i and %l registers will be reloaded from the place to
 * which %sp points, due to sigreturn() semantics (sigreturn does not modify
 * the window pointer in the psr, hence it must force all windows to reload).
 */
ENTRY(longjmp)
	save	%sp, -CC64FSZ, %sp
	ldx	[%i0 + 0x08], %o2	/* make sure sc->sc_sp, sc->sc_fp nonzero */
	ldx	[%i0 + 0x28], %o3
	orcc	%o2, %o3, %g0
	bz,pn	%xcc, Lbotch
	 movrz	%i1, 1, %i1		/* if (v == 0) v = 1; */
	st	%i1, [%i0 + 0x30]	/* sc.sc_o0 = v; */
	mov	SYS_compat_13_sigreturn13, %g1
	mov	%i0, %o0
	t	ST_SYSCALL		/* sigreturn(scp); */

Lbotch:
	/* oops, caller botched it */
	call	_C_LABEL(longjmperror)
	 nop
	unimp	0
