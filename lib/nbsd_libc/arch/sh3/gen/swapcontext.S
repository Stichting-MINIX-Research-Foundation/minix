/*	$NetBSD: swapcontext.S,v 1.9 2008/04/28 20:22:57 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: swapcontext.S,v 1.9 2008/04/28 20:22:57 martin Exp $")
#endif /* SYSLIBC_SCCS and not lint */

/*
 * int
 * swapcontext(ucontext_t * restrict oucp, ucontext_t * restrict ucp);
 */
ENTRY(swapcontext)
	PIC_PROLOGUE(.L_got)
	mov.l	r5, @-sp
	sts.l	pr, @-sp

	mov.l	.L__getcontext, r0
1:	CALL	r0			/* _getcontext(oucp) */
	 mov.l	r4, @-sp
	mov.l	@sp+, r1
	tst	r0, r0
	bf	3f			/* return error from getcontext */

	/* Note: getcontext does _UC_MACHINE_INTRV(oucp) = 0 for us */
	mov.l	@sp, r0
	mov.l	r0, @(36 + 1 * 4, r1)	/* _UC_MACHINE_SET_PC(oucp, pr) */
	mov.l	.L_setcontext, r2
2:	CALL	r2			/* setcontext(ucp) */
	 mov.l	@(4, sp), r4
	/* if we get here, return error from setcontext */
3:
	lds.l	@sp+, pr
#ifdef PIC
	add	#4, sp
	rts
	 PIC_EPILOGUE
#else
	rts
	 add	#4, sp
#endif

	.align	2
.L_got:			PIC_GOT_DATUM
.L__getcontext:		CALL_DATUM(_C_LABEL(_getcontext), 1b)
.L_setcontext:		CALL_DATUM(_C_LABEL(setcontext), 2b)
	SET_ENTRY_SIZE(swapcontext)
