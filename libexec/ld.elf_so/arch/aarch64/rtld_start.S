/* $NetBSD: rtld_start.S,v 1.1 2014/08/10 05:47:37 matt Exp $ */

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

RCSID("$NetBSD: rtld_start.S,v 1.1 2014/08/10 05:47:37 matt Exp $")

/*
 * void _rtld_start(void (*cleanup)(void), const Obj_Entry *obj,
 *    struct ps_strings *ps_strings);
 *
 * X0		= NULL
 * X1		= NULL
 * X2		= ps_strings
 * X30 (LR)	= 0
 * X29 (FP)	= 0
 */

ENTRY_NP(_rtld_start)
	mov	x24, x2			/* save ps_strings */

#if 1
	adrp	x1, _PROCEDURE_LINKAGE_TABLE_	/* load _DYNAMIC offset from GOT */
	ldr	x1, [x1, #:got_lo12:_PROCEDURE_LINKAGE_TABLE_]
#else
	adrp	x1, :got:_DYNAMIC	/* load _DYNAMIC offset from GOT */
	ldr	x1, [x1, #:got_lo12:_DYNAMIC]
#endif

	adrp	x0, _DYNAMIC		/* get &_DYNAMIC */
	add	x0, x0, #:lo12:_DYNAMIC
	sub	x25, x0, x1		/* relocbase = &_DYNAMIC - GOT:_DYNAMIC */
	mov	x1, x25			/* pass as 2nd argument */
	bl	_rtld_relocate_nonplt_self

	sub	sp, sp, #16		/* reserve space for returns */
	mov	x0, sp			/* pointer to reserved space */
	mov	x1, x25			/* pass relocbase */
	bl	_rtld
	mov	x17, x0			/* save entry point */

	ldp	x0, x1, [sp], #16	/* pop cleanup & obj_main */
	mov	x2, x24			/* restore ps_strings */

	br	x17			/* call saved entry point */
END(_rtld_start)

/*
 * Upon entry from plt0 entry:
 * X17 = &PLTGOT[n]
 * X16 = &PLTGOT[2]
 */
ENTRY_NP(_rtld_bind_start)
	sub	sp, sp, #96		/* reserve stack space */
	stp	x29, x30, [sp, #80]	/* save FP & LR */
	add	x29, sp, #80		/* get new FP */
	str	x24, [sp, #64]		/* save caller-saved register */
	stp	x6, x7, [sp, #48]	/* save arguments */
	stp	x4, x5, [sp, #32]	/* save arguments */
	stp	x2, x3, [sp, #16]	/* save arguments */
	stp	x0, x1, [sp, #0]	/* save arguments */

	sub	x16, x16, #16		/* adjust to &PLTGOT[0] */
	mov	x24, x17		/* preserve across _rtld_bind */
	sub	x1, x17, x16		/* x1 = &PLTGOT[N] - &PLTGOT[2] */
	lsr	x1, x1, #3		/* x1 = N - 2 */
	ldr	x0, [x16, #8]		/* get obj ptr from &PLTGOT[1] */
	bl	_rtld_bind
	str	x0, [x24]		/* save address in PLTGOT[N] */
	mov	x17, x0			/* save address */

	ldp	x0, x1, [sp, #0]	/* restore arguments */
	ldp	x2, x3, [sp, #16]	/* restore arguments */
	ldp	x4, x5, [sp, #32]	/* restore arguments */
	ldp	x6, x7, [sp, #48]	/* restore arguments */
	ldr	x24, [sp, #64]		/* save caller-saved register */
	ldp	x29, x30, [sp, #80]	/* restore FP & LR */
	add	sp, sp, #96		/* reclaim stack */
	br	x17			/* call bound function */
END(_rtld_bind_start)
