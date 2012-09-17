/*	$NetBSD: _setjmp.S,v 1.7 2011/04/30 19:43:17 martin Exp $	*/

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
 * from: Header: _setjmp.s,v 1.1 91/07/06 16:45:53 torek Exp
 */

#include <machine/asm.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	.asciz "@(#)_setjmp.s	8.1 (Berkeley) 6/4/93"
#else
	RCSID("$NetBSD: _setjmp.S,v 1.7 2011/04/30 19:43:17 martin Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

	.register %g2,#ignore
	.register %g3,#ignore

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v?v:1)" from
 * the last call to
 *	_setjmp(a)
 * The previous signal state is NOT restored.
 */

ENTRY(_setjmp)
	stx	%sp, [%o0+0]	/* store caller's stack pointer */
	stx	%o7, [%o0+8]	/* and return pc */
	stx	%g0, [%o0+0x18]	/* mark as non-sigmask jmpbuf */

	/* store globals, offsets compatible with siglongjmp */
	stx	%g1, [%o0+0x28]
	stx	%g2, [%o0+0x30]		/* adjust asserts in longjmp.c */
	stx	%g3, [%o0+0x48]		/* if you change any of these */
	stx	%g6, [%o0+0x50]		/* offsets! */
	stx	%g7, [%o0+0x58]

	retl
	 clr	%o0		! return 0

ENTRY(_longjmp)
	save	%sp, -CC64FSZ, %sp
	flushw
	/*
	 * We restore the saved stack pointer to %fp, then issue
	 * a `restore' instruction which will reload the register
	 * window from the stack.
	 */
	ldx	[%i0+0x28],%g1
	ldx	[%i0+0x30],%g2
	ldx	[%i0+0x48],%g3
	ldx	[%i0+0x50],%g6
	ldx	[%i0+0x58],%g7
	ldx	[%i0+8], %i7
	ldx	[%i0+0], %fp
	mov	1, %i0
	movrnz	%i1, %i1, %i0	! compute v ? v : 1
	ret
	 restore
