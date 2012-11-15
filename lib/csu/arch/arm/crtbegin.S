/*	$NetBSD: crtbegin.S,v 1.4 2012/08/25 15:39:05 matt Exp $	*/
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

RCSID("$NetBSD: crtbegin.S,v 1.4 2012/08/25 15:39:05 matt Exp $")

	.section	.ctors, "aw", %progbits
	.p2align 2
__CTOR_LIST__:
	.word -1

	.section	.dtors, "aw", %progbits
	.p2align 2
__DTOR_LIST__:
	.word -1

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

	.local	__dwarf_eh_object
	.comm	__dwarf_eh_object,24
	.local	__initialized
	.comm	__initialized,1
	.local	__finished
	.comm	__finished,1

	.text
/*
 * All variables are local to this DSO so we can skip using GOT references
 * and instead use PCREL references to access them.  We do this regardless
 * of being PIC since it isn't any additional overhead to do so.
 *
 * We don't setup a TOC since all of ours calls are indirect so it isn't
 * needed.
 */

	.type __do_global_dtors_aux, %function
__do_global_dtors_aux:
	GOT_INIT(r3, .Ldtors_got, .Ldtors) 	/* use r3 temporarily */
	ldr	r1, .L__finished
#ifdef PIC
        ldrb	r0, [r1, r3]!
#else
        ldrb	r0, [r1]
#endif
	cmp	r0, #0				/* done this already? */
	RETc(ne)
	mov	r0, #1
	strb	r0, [r1]			/* mark it as done */

	mov     ip, sp
        stmfd   sp!, {r4, r5, r6, r7, fp, ip, lr, pc}
        sub     fp, ip, #4
	mov	r7, r3				/* now that r7 is saved ... */

#ifdef SHARED
	GOT_GET(ip, r7, .L__cxa_finalize)
	cmp	ip, #0
	ldrne	r2, .L__dso_handle		/* GOTOFF */
	ldrne	r0, [r2, r7]			/* load relative to GOT */
#ifdef _ARM_ARCH_4T
	blxne	ip
#else
	movne	lr, pc
	movne	pc, ip
#endif
#endif /* SHARED */

	/*
	 * We know the first entry is -1 so skip it.  We use load with
	 * preincrement to advance the pointer along.
	 */
	ldr	r4, .L__DTOR_LIST__
	ldr	r5, .L__DTOR_LIST_END__
#ifdef PIC
	add	r4, r4, r7		/* addr = offset + got */
	add	r5, r5, r7		/* addr = offset + got */
#endif
	add	r4, r4, #4		/* skip first entry */
1:
	cmp	r4, r5			/* end of list */
	ldrne	ip, [r4], #4		/* ip = *r4++; */
#ifndef SHARED
        ldmeqfd	sp, {r4, r5, r6, r7, fp, sp, pc} /* restore and return */
#endif
	adrne	lr, 1b			/* set up to return to loop start */
#ifdef _ARM_ARCH_4T
	bxne	ip			/* call it */
#else
	movne	pc, ip			/* call entry */
#endif

#ifdef SHARED
	/*
	 * if (__deregister_frame_info)
	 *     __deregister_frame_info(&__EH_FRAME_LIST__[0]);
	 */
	GOT_GET(ip, r7, .L__deregister_frame_info)
	cmp	ip, #0
        ldmeqfd	sp, {r4-r7, fp, sp, pc}		/* restore and return */

	ldr	r1, .L__EH_FRAME_LIST__
#ifdef PIC
	add	r0, r1, r7			/* add offset to GOT addr */
#endif
        ldmfd	sp, {r4-r7, fp, sp, lr}		/* restore everthing */
#ifdef _ARM_ARCH_4T
	bx	ip				/* tail call it */
#else
	mov	pc, ip				/* tail call it */
#endif
#endif /* SHARED */

	.weak	__deregister_frame_info
	.weak	__cxa_finalize
	.hidden	_C_LABEL(__DTOR_LIST_END__)

	.align	2
	GOT_INITSYM(.Ldtors_got, .Ldtors)
#ifdef SHARED
.L__deregister_frame_info:
        .word   PIC_SYM(_C_LABEL(__deregister_frame_info), GOT)
.L__cxa_finalize:
        .word   PIC_SYM(_C_LABEL(__cxa_finalize), GOT)
.L__dso_handle:
        .word   PIC_SYM(_C_LABEL(__dso_handle), GOTOFF)
#endif
.L__finished:
        .word   PIC_SYM(_C_LABEL(__finished), GOTOFF)
.L__DTOR_LIST__:
        .word   PIC_SYM(_C_LABEL(__DTOR_LIST__), GOTOFF)
.L__DTOR_LIST_END__:
        .word   PIC_SYM(_C_LABEL(__DTOR_LIST_END__), GOTOFF)
.L__EH_FRAME_LIST__:
        .word   PIC_SYM(_C_LABEL(__EH_FRAME_LIST__), GOTOFF)


	.weak	__register_frame_info
	.weak	_Jv_RegisterClasses

	.type __do_global_ctors_aux, %function
__do_global_ctors_aux:
	GOT_INIT(r3, .Lctors_got, .Lctors) 
	ldr	r1, .L__initialized
#ifdef PIC
        ldrb	r0, [r1, r3]!
#else
        ldrb	r0, [r1]
#endif
	cmp	r0, #0				/* done this already? */
	RETc(ne)
	mov	r0, #1
	strb	r0, [r1]			/* mark it as done */

	mov     ip, sp
        stmfd   sp!, {r4, r5, r6, r7, fp, ip, lr, pc}
        sub     fp, ip, #4
	mov	r7, r3				/* now that r7 is saved ... */

	/*
	 * if (__register_frame_info)
	 *     __register_frame_info(&__EH_FRAME_LIST__[0], &__dwarf_eh_object)
	 */
	GOT_GET(ip, r7, .L__register_frame_info)
	cmp	ip, #0
	beq	1f

	ldr	r0, .L__EH_FRAME_LIST__
	ldr	r1, .L__dwarf_eh_object
#ifdef PIC
	add	r0, r0, r7		/* object addr = got addr + offset */
	add	r1, r1, r7		/* object addr = got addr + offset */
#endif

#ifdef _ARM_ARCH_4T
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
	GOT_GET(ip, r7, .L_Jv_RegisterClasses)
	cmp	ip, #0

	ldrne	r0, .L__JCR_LIST__
#ifdef PIC
	ldrne	r2, [r0, r7]!		/* load 1st one, make r0 point to it */
#else
	ldrne	r2, [r0]		/* load 1st one */
#endif
	cmpne	r2, #0
#ifdef _ARM_ARCH_4T
	blxne	ip
#else
	movne	lr, pc
	movne	pc, ip
#endif

	/*
	 * Get the start and end of the CTOR list.
	 */
	
	ldr	r4, .L__CTOR_LIST__
	ldr	r5, .L__CTOR_LIST_END__
#ifdef PIC
	add	r4, r4, r7		/* object addr = got addr + offset */
	add	r5, r5, r7		/* object addr = got addr + offset */
#endif
	sub	r5, r5, #4		/* ignore first entry */
3:
	cmp	r4, r5			/* at first entry? */
        ldmeqfd	sp, {r4-r7, fp, sp, pc} /* then return */
	adr	lr, 3b			/* return to start of loop */
	ldr	ip, [r5], #-4		/* get entry */
#ifdef _ARM_ARCH_4T
	bx	ip
#else
	mov	pc, ip				/* get entry */
#endif

	.hidden	_C_LABEL(__CTOR_LIST_END__)

        .align  2
	GOT_INITSYM(.Lctors_got, .Lctors)
.L_Jv_RegisterClasses:
        .word   PIC_SYM(_C_LABEL(_Jv_RegisterClasses), GOT)
.L__register_frame_info:
        .word   PIC_SYM(_C_LABEL(__register_frame_info), GOT)
.L__initialized:
        .word   PIC_SYM(_C_LABEL(__initialized), GOTOFF)
.L__CTOR_LIST__:
        .word   PIC_SYM(_C_LABEL(__CTOR_LIST__), GOTOFF)
.L__CTOR_LIST_END__:
        .word   PIC_SYM(_C_LABEL(__CTOR_LIST_END__), GOTOFF)
.L__dwarf_eh_object:
        .word   PIC_SYM(_C_LABEL(__dwarf_eh_object), GOTOFF)
.L__JCR_LIST__:
        .word   PIC_SYM(_C_LABEL(__JCR_LIST__), GOTOFF)

	.section	.init, "ax", %progbits
	bl	__do_global_ctors_aux
	.section	.fini, "ax", %progbits
	bl	__do_global_dtors_aux
