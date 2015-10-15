/*	$NetBSD: rtld_start.S,v 1.12 2013/12/03 00:19:56 matt Exp $	*/

/*-
 * Copyright (c) 1998, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas and by Charles M. Hannum.
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

RCSID("$NetBSD: rtld_start.S,v 1.12 2013/12/03 00:19:56 matt Exp $")

ENTRY_NP(_rtld_start)
	sub	sp, sp, #8		/* make room for obj_main & exit proc */
	movs	r4, r0			/* save ps_strings */

	ldr	r6, .Lgot
	ldr	r5, .Ldynamic
	add	r6, r6, pc		/* r6 = &GOT[0]; pc = &LPIC1; */
	adds	r0, r6, r5		/* arg0 = &_DYNAMIC */
.LPIC1:
	ldr	r5, [r6]		/* GOT[0] = &_DYNAMIC */
	subs	r5, r0, r5		/* relocbase = GOT[0] - &_DYNAMIC */
	movs	r1, r5			/* arg1 = relocabase */
	bl	_rtld_relocate_nonplt_self

	movs	r1, r5			/* relocbase */
	mov	r0, sp			/* sp */
	bl	_rtld			/* call the shared loader */
	movs	r3, r0			/* save entry point */

#ifdef __thumb__
	ldr	r2, [sp, #0]		/* pop r2 = cleanup */
	ldr	r1, [sp, #4]		/* pop r1 = obj_main */
	add	sp, sp, #8
#else
	ldr	r2, [sp], #4		/* pop r2 = cleanup */
	ldr	r1, [sp], #4		/* pop r1 = obj_main */
#endif
	movs	r0, r4			/* restore ps_strings */
	RETr(r3)

	.align	0
.Lgot:
	.word	_GLOBAL_OFFSET_TABLE_ - .LPIC1
.Ldynamic:
	.word	_DYNAMIC(GOTOFF)
END(_rtld_start)

/*
 *	stack[0] = RA
 *	ip = &GOT[n+3]
 *	lr = &GOT[2]
 */
ARM_ENTRY_NP(_rtld_bind_start)
	stmdb	sp!,{r0-r4,sl,fp}	/* 8 byte aligned (lr already saved) */

	sub	r1, ip, lr		/* r1 = 4 * (n + 1) */
	sub	r1, r1, #4		/* r1 = 4 * n */
	add	r1, r1, r1		/* r1 = 8 * n */

	ldr	r0, [lr, #-4]		/* get obj ptr from GOT[1] */
	mov	r4, ip			/* save GOT location */

	bl	_rtld_bind		/* Call the binder */

	str	r0, [r4]		/* save address in GOT */
	mov	ip, r0			/* save new address */

	ldmia	sp!,{r0-r4,sl,fp,lr}	/* restore the stack */
	RETr(ip)			/* jump to the new address */
END(_rtld_bind_start)
