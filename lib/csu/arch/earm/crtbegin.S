/*	$NetBSD: crtbegin.S,v 1.1 2012/08/13 02:49:04 matt Exp $	*/
/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include <arm/asm.h>

RCSID("$NetBSD: crtbegin.S,v 1.1 2012/08/13 02:49:04 matt Exp $")

	.section	.eh_frame, "a", %progbits
	.p2align 2
__EH_FRAME_LIST__:

	.section	.jcr, "aw", %progbits
	.p2align 2
__JCR_LIST__:

	.section	.data.rel, "aw", %progbits
	.p2align 2
	.type	__dso_handle, %object
	.size	__dso_handle, 4
	.globl	__dso_handle
	.hidden	__dso_handle
__dso_handle:
#ifdef SHARED
	.word	__dso_handle
#else
	.word	0
#endif

	.text
#ifdef SHARED
	/*
	 * If we aren't shared, all we do is set finished
	 * to 1 if it isn't 1 already.  So why bother do that at all?
	 */
	.type __do_global_dtors_aux, %function
__do_global_dtors_aux:
	GOT_INIT(r3, .Ldtors_got, .Ldtors) 	/* use r3 temporarily */
	ldr	r1, .L__finished		/* GOTOFF */
	ldrb	r0, [r1, r3]!
	cmp	r0, #0				/* done this already? */
	RETc(ne)
	mov	r0, #1
	strb	r0, [r1]			/* mark it as done */

	GOT_GET(ip, r3, .L__cxa_finalize)
	cmp	ip, #0
	RETc(eq)
	ldr	r0, .L__dso_handle		/* GOTOFF */
	ldr	r0, [r0, r3]			/* load relative to got */
#if defined(_ARM_ARCH_4T)
	bx	ip				/* tail call */
#else
	mov	pc, ip
#endif

#if 0	/* not used */
	/*
	 * if (__deregister_frame_info)
	 *     __deregister_frame_info(&__EH_FRAME_LIST__[0]);
	 */
	GOT_GET(ip, r3, .L__deregister_frame_info)
	cmp	ip, #0
	ldmeqfd	sp, {r4, r5, fp, sp, pc}	/* restore and return */

	ldr	r1, .L__EH_FRAME_LIST__		/* GOTOFF */
	add	r1, r1, r4			/* add got addr */
	ldmfd	sp, {r4, r5, fp, sp, lr}	/* restore everthing */
#if !defined(_ARM_ARCH_4T)
	bx	ip				/* tail call it */
#else
	mov	pc, ip				/* tail call it */
#endif
#endif /* notused */

	.weak	__cxa_finalize
	.weak	__deregister_frame_info

	.align	2
	GOT_INITSYM(.Ldtors_got, .Ldtors)
.L__cxa_finalize:
	.word	PIC_SYM(_C_LABEL(__cxa_finalize), GOT)
#if 0
.L__deregister_frame_info:
	.word	PIC_SYM(_C_LABEL(__deregister_frame_info), GOT)
#endif
.L__finished:
	.word	PIC_SYM(_C_LABEL(__finished), GOTOFF)
.L__dso_handle:
	.word	PIC_SYM(_C_LABEL(__dso_handle), GOTOFF)

	.pushsection .fini_array,"aw",%fini_array
	.p2align 2
	.word	__do_global_dtors_aux
	.popsection
#endif /* SHARED */

	.local	__finished
	.comm	__finished,1,1

	.weak	__register_frame_info
	.weak	_Jv_RegisterClasses

	.type __do_global_ctors_aux, %function
__do_global_ctors_aux:
	GOT_INIT(r3, .Lctors_got, .Lctors) 
	ldr	r1, .L__initialized		/* GOTOFF if PIC */
#ifdef PIC
	ldrb	r0, [r1, r3]!			/* r1 = r1 + got */
#else
	ldrb	r0, [r1]
#endif
	cmp	r0, #0				/* done this already? */
	RETc(ne)
	mov	r0, #1
	strb	r0, [r1]			/* mark it as done */

	mov	ip, sp
	stmfd	sp!, {r4, r5, fp, ip, lr, pc}
	sub	fp, ip, #4
	mov	r4, r3				/* now that r4 is saved ... */

	/*
	 * if (__register_frame_info)
	 *     __register_frame_info(&__EH_FRAME_LIST__[0], &__dwarf_eh_object)
	 */
	GOT_GET(ip, r4, .L__register_frame_info)
	cmp	ip, #0
	beq	1f

	ldr	r0, .L__EH_FRAME_LIST__
	ldr	r1, .L__dwarf_eh_object
#ifdef PIC
	add	r0, r0, r4
	add	r1, r1, r4
#endif

#if defined(_ARM_ARCH_4T)
	blx	ip
#else
	mov	lr, pc
	mov	pc, ip
#endif
1:
	/*
	 * if (_Jv_RegisterClasses && __JCR_LIST__[0])
	 *     _Jv_RegisterClasses(&__JCR_LIST__[0]);
	 */
	GOT_GET(ip, r4, .L_Jv_RegisterClasses)
	cmp	ip, #0
	ldmeqfd	sp, {r4, r5, fp, sp, pc}	/* return if null */

	ldr	r0, .L__JCR_LIST__
#ifdef PIC
	ldr	r2, [r0, r4]!			/* GOTOFF + got == address */
#else
	ldr	r2, [r0]
#endif
	cmp	r2, #0
	ldmfd	sp, {r4, r5, fp, sp, lr}	/* restore everything */
	RETc(eq)				/* return if null */
#if defined(_ARM_ARCH_4T)
	bx	ip		/* return or tail call */
#else
	mov	pc, ip		/* return or tail call */
#endif

	.p2align 2
	GOT_INITSYM(.Lctors_got, .Lctors)
.L__register_frame_info:
	.word	PIC_SYM(_C_LABEL(__register_frame_info), GOT)
.L_Jv_RegisterClasses:
	.word	PIC_SYM(_C_LABEL(_Jv_RegisterClasses), GOT)
.L__initialized:
	.word	PIC_SYM(_C_LABEL(__initialized), GOTOFF)
.L__EH_FRAME_LIST__:
	.word	PIC_SYM(_C_LABEL(__EH_FRAME_LIST__), GOTOFF)
.L__dwarf_eh_object:
	.word	PIC_SYM(_C_LABEL(__dwarf_eh_object), GOTOFF)
.L__JCR_LIST__:
	.word	PIC_SYM(_C_LABEL(__JCR_LIST__), GOTOFF)

	.local	__initialized
	.comm	__initialized,1,1
	.local	__dwarf_eh_object
	.comm	__dwarf_eh_object,24,4

	.section .init_array,"aw",%init_array
	.p2align 2
	.word	__do_global_ctors_aux
