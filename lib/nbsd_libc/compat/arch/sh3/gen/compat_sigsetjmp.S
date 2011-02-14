/*	$NetBSD: compat_sigsetjmp.S,v 1.5 2006/01/05 19:21:37 uwe Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)setjmp.s	5.1 (Berkeley) 4/23/90"
 */

#include <machine/asm.h>
#include <machine/setjmp.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: compat_sigsetjmp.S,v 1.5 2006/01/05 19:21:37 uwe Exp $")
#endif

ENTRY(sigsetjmp)
	tst	r5, r5
	mov.l	r5, @(_JB_HAS_MASK * 4, r4)
	bt	2f			/* if (savemask == 0) */

	PIC_PROLOGUE(.L_got_1)
	sts.l	pr, @-sp
	mov.l	r4, @-sp

	mov.l	.L_sigblock, r0
1:	CALL	r0			/* int mask = sigblock(0); */
	 mov	#0, r4

	mov.l	@sp+, r4
	lds.l	@sp+, pr
	PIC_EPILOGUE

	mov.l	r0, @(_JB_SIGMASK * 4, r4)

2:	/* identical to _setjmp sans mask */
	add	#((_JB_REG_R15 + 1) * 4), r4
	mov.l	r15, @-r4
	mov.l	r14, @-r4
	mov.l	r13, @-r4
	mov.l	r12, @-r4
	mov.l	r11, @-r4
	mov.l	r10, @-r4
	mov.l	r9, @-r4
	mov.l	r8, @-r4
	sts.l	pr, @-r4
	rts
	 xor	r0, r0

	.align	2
.L_got_1:	PIC_GOT_DATUM
.L_sigblock:	CALL_DATUM(_C_LABEL(sigblock), 1b)
	SET_ENTRY_SIZE(sigsetjmp)


ENTRY(siglongjmp)
	mov.l	@(_JB_HAS_MASK * 4, r4), r0
	tst	r0, r0
	bt	2f			/* if no mask */

	/* we won't return here, so we don't need to save pr and r12 */
	PIC_PROLOGUE_NOSAVE(.L_got_2)
	mov.l	r5, @-sp
	mov.l	r4, @-sp

	mov.l	.L_sigsetmask, r0
1:	CALL	r0			/* sigsetmask(saved mask) */
	 mov.l	@(_JB_SIGMASK * 4, r4), r4

	mov.l	@sp+, r4
	mov.l	@sp+, r5

2:	/* identical to _longjmp */
	lds.l	@r4+, pr
	mov.l	@r4+, r8
	mov.l	@r4+, r9
	mov.l	@r4+, r10
	mov.l	@r4+, r11
	mov.l	@r4+, r12
	mov.l	@r4+, r13
	mov.l	@r4+, r14
	mov.l	@r4+, r15

	mov	r5, r0
	tst	r0, r0	/* make sure return value is non-zero */
	bf	.L0
	add	#1, r0
.L0:
	rts
	 nop

	.align	2
.L_got_2:	PIC_GOT_DATUM
.L_sigsetmask:	CALL_DATUM(_C_LABEL(sigsetmask), 1b)
	SET_ENTRY_SIZE(siglongjmp)
