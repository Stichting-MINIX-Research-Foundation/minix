/*	$NetBSD: compat___sigtramp1.S,v 1.1 2005/10/15 22:11:22 uwe Exp $	*/

/*
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its contributors
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

#include "SYS.h"

#define _LOCORE
#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/signal.h>

/*
 * When this code is run, the stack looks like:
 *	[%sp]		64 bytes to which registers can be dumped
 *	[%sp + 64]	signal number (goes in %o0)
 *	[%sp + 64 + 4]	signal code (goes in %o1)
 *	[%sp + 64 + 8]	placeholder
 *	[%sp + 64 + 12]	argument for %o3, currently unsupported (always 0)
 *	[%sp + 64 + 16]	first word of saved state (sigcontext)
 *	    .
 *	    .
 *	    .
 *	[%sp + NNN]	last word of saved state
 * (followed by previous stack contents or top of signal stack).
 * The address of the function to call is in %g1; the old %g1 and %o0
 * have already been saved in the sigcontext.  We are running in a clean
 * window, all previous windows now being saved to the stack.
 *
 * Note that [%sp + 64 + 8] == %sp + 64 + 16.  The copy at %sp+64+8
 * will eventually be removed, with a hole left in its place, if things
 * work out.
 */
#define SAVE_STATE \
	/* \
	 * XXX  the `save' and `restore' below are unnecessary: should \
	 *	replace with simple arithmetic on %sp \
	 * \
	 * Make room on the stack for 32 %f registers + %fsr.  This comes \
	 * out to 33*4 or 132 bytes, but this must be aligned to a multiple \
	 * of 8, or 136 bytes. \
	 */ \
	save	%sp, -CCFSZ - 136, %sp; \
	mov	%g2, %l2;		/* save globals in %l registers */ \
	mov	%g3, %l3; \
	mov	%g4, %l4; \
	mov	%g5, %l5; \
	mov	%g6, %l6; \
	mov	%g7, %l7; \
	/* \
	 * Saving the fpu registers is expensive, so do it iff the fsr \
	 * stored in the sigcontext shows that the fpu is enabled. \
	 */ \
	ld	[%fp + 64 + 16 + SC_PSR_OFFSET], %l0; \
	sethi	%hi(PSR_EF), %l1;	/* FPU enable is too high for andcc */ \
	andcc	%l0, %l1, %l0;		/* %l0 = fpu enable bit */ \
	be	1f;			/* if not set, skip the saves */ \
	 rd	%y, %l1;		/* in any case, save %y */ \
	/* fpu is enabled, oh well */ \
	st	%fsr, [%sp + CCFSZ + 0]; \
	std	%f0, [%sp + CCFSZ + 8]; \
	std	%f2, [%sp + CCFSZ + 16]; \
	std	%f4, [%sp + CCFSZ + 24]; \
	std	%f6, [%sp + CCFSZ + 32]; \
	std	%f8, [%sp + CCFSZ + 40]; \
	std	%f10, [%sp + CCFSZ + 48]; \
	std	%f12, [%sp + CCFSZ + 56]; \
	std	%f14, [%sp + CCFSZ + 64]; \
	std	%f16, [%sp + CCFSZ + 72]; \
	std	%f18, [%sp + CCFSZ + 80]; \
	std	%f20, [%sp + CCFSZ + 88]; \
	std	%f22, [%sp + CCFSZ + 96]; \
	std	%f24, [%sp + CCFSZ + 104]; \
	std	%f26, [%sp + CCFSZ + 112]; \
	std	%f28, [%sp + CCFSZ + 120]; \
	std	%f30, [%sp + CCFSZ + 128]; \
1:

#define RESTORE_STATE \
	/* \
	 * Now that the handler has returned, re-establish all the state \
	 * we just saved above, then do a sigreturn. \
	 */ \
	tst	%l0;			/* reload fpu registers? */ \
	be	1f;			/* if not, skip the loads */ \
	 wr	%l1, %g0, %y;		/* in any case, restore %y */ \
	ld	[%sp + CCFSZ + 0], %fsr; \
	ldd	[%sp + CCFSZ + 8], %f0; \
	ldd	[%sp + CCFSZ + 16], %f2; \
	ldd	[%sp + CCFSZ + 24], %f4; \
	ldd	[%sp + CCFSZ + 32], %f6; \
	ldd	[%sp + CCFSZ + 40], %f8; \
	ldd	[%sp + CCFSZ + 48], %f10; \
	ldd	[%sp + CCFSZ + 56], %f12; \
	ldd	[%sp + CCFSZ + 64], %f14; \
	ldd	[%sp + CCFSZ + 72], %f16; \
	ldd	[%sp + CCFSZ + 80], %f18; \
	ldd	[%sp + CCFSZ + 88], %f20; \
	ldd	[%sp + CCFSZ + 96], %f22; \
	ldd	[%sp + CCFSZ + 104], %f24; \
	ldd	[%sp + CCFSZ + 112], %f26; \
	ldd	[%sp + CCFSZ + 120], %f28; \
	ldd	[%sp + CCFSZ + 128], %f30; \
1: \
	mov	%l2, %g2; \
	mov	%l3, %g3; \
	mov	%l4, %g4; \
	mov	%l5, %g5; \
	mov	%l6, %g6; \
	mov	%l7, %g7

ENTRY_NOPROFILE(__sigtramp_sigcontext_1)
	SAVE_STATE

	ldd	[%fp + 64], %o0		/* sig, code */
	ld	[%fp + 76], %o3		/* arg3 */
	call	%g1			/* call handler */
	 add	%fp, 64 + 16, %o2	/* pointer to sigcontext */

	RESTORE_STATE

	/* Get registers back and set syscall # */
	restore	%g0, SYS_compat_16___sigreturn14, %g1
	add	%sp, 64 + 16, %o0	/* compute scp */
	t	ST_SYSCALL		/* call sigreturn */
	mov	SYS_exit, %g1		/* exit with errno */
	t	ST_SYSCALL		/* if sigreturn fails */
