/* $NetBSD: swapcontext.S,v 1.1 2014/08/10 05:47:36 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
        RCSID("$NetBSD: swapcontext.S,v 1.1 2014/08/10 05:47:36 matt Exp $")
#endif /* LIBC_SCCS && !lint */

/* LINTSTUB: int swapcontext(ucontext_t * restrict, ucontext_t * restrict); */

ENTRY(swapcontext)
	sub	sp, sp, #32		/* allocate stack frame */
	stp	x29, x30, [sp, #16]	/* save FP & RA */
	add	x29, sp, #16		/* new FP */
	stp	x0, x1, [sp, #0]	/* save oucp & ucp */

	bl	_C_LABEL(_getcontext)	/* getcontext(oucp) */
	mov	x3, x0			/* save return value */

	ldp	x2, x0, [sp, #0]	/* restore oucp & ucp */
	ldp	x29, x30, [sp, #16]	/* restore FP & RA */
	add	sp, sp, #32		/* free stack frame */

	/*
	 * Assume _getcontext succeeded.
	 */
	mov	x4, sp			/* Get SP */
	str	x4, [x2, #_UC_REGS_SP]	/* Adjust saved SP. */
	str	x30, [x2, #_UC_REGS_PC]	/* Adjust saved LR. */

	cbz	x3, _C_LABEL(setcontext)/* setcontext if getcontext succeeded */
	mov	x0, x3			/* restore getcontext return value */
	ret				/* return on error from getcontext */
END(swapcontext)
