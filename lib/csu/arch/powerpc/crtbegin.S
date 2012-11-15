/*	$NetBSD: crtbegin.S,v 1.2 2011/07/04 21:55:09 matt Exp $	*/
/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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

#include <powerpc/asm.h>

RCSID("$NetBSD: crtbegin.S,v 1.2 2011/07/04 21:55:09 matt Exp $")

	.section	.ctors, "aw", @progbits
	.p2align 2
__CTOR_LIST__:
	.long -1

	.section	.dtors, "aw", @progbits
	.p2align 2
__DTOR_LIST__:
	.long -1

	.section	.eh_frame, "a", @progbits
	.p2align 2
__EH_FRAME_LIST__:

	.section	.jcr, "aw", @progbits
	.p2align 2
__JCR_LIST__:

	.section	".got2","aw", @progbits
.LCTOC = . + 32768

	.section	.data.rel, "aw", @progbits
	.p2align 2
	.type	__dso_handle, @object
	.size	__dso_handle, 4
	.globl	__dso_handle
	.hidden	__dso_handle
__dso_handle:
#ifdef SHARED
	.long	__dso_handle
#else
	.long	0
#endif

	.lcomm	__dwarf_eh_object,8*SZREG,SZREG
	.lcomm	__initialized,1,1
	.lcomm	__finished,1,1

	.text
	.weak	__cxa_finalize
	.weak	__deregister_frame_info
	.weak	__register_frame_info
	.weak	_Jv_RegisterClasses

/*
 * All variables are local to this DSO so we can skip using GOT references
 * and instead use PCREL references to access them.  We do this regardless
 * of being PIC since it isn't any additional overhead to do so.
 *
 * We don't setup a TOC since all of ours calls are indirect so it isn't
 * needed.
 */

__do_global_dtors_aux:
	mflr	%r7				/* save return address */
	bcl     20,31,1f			/* find out what address */
1:	mflr    %r3				/*   we are at */
	mtlr	%r7				/* restore return address */

	addis   %r4,%r3,__finished-1b@ha	/* PCREL ref (upper) */
	lbzu	%r0,__finished-1b@l(%r4)	/* PCREL ref (lower) & load */
	cmpwi	%r0,0				/* done this already? */
	bnelr					/*   yep, return */

	li	%r0, 1				
	stb	%r0, 0(%r4)			/* remember we've done this */

	stw	%r7,(1*SZREG)(%r1)		/* save LR */
	stwu	%r1,-(8*SZREG)(%r1)		/* alloc our stack frame */
	stw	%r31,(7*SZREG)(%r1)		/* save some registers */
	stw	%r30,(6*SZREG)(%r1)
	stw	%r29,(5*SZREG)(%r1)
	stw	%r28,(4*SZREG)(%r1)

	mr	%r29,%r3			/* move this to a safe place */

	/*
	 * Get a reference to our TOC.
	 */
	addis   %r30,%r29,.LCTOC-1b@ha
	addi    %r30,%r30,.LCTOC-1b@l

	/*
	 * Get a reference to the GOT.
	 */
	addis	%r31,%r29,_GLOBAL_OFFSET_TABLE_-1b@ha	/* pcrel (hi) */
	addi	%r31,%r31,_GLOBAL_OFFSET_TABLE_-1b@l	/* pcrel (lo) */

#ifdef SHARED
	/*
	 * if (__cxa_finalize)
	 *     __cxa_finalize(&__dso_handle);
	 */
	lwz	%r0, __cxa_finalize@got(%r31)
	cmpwi	%r0, 0
	beq	2f

	addis	%r3,%r29,__dso_handle-1b@ha	/* get &__dso_handle (pcrel) */
	addi	%r3,%r3,__dso_handle-1b@l
	mtctr	%r0				/* setup for indirect branch */
	bctrl					/* and call the routine */
2:
#endif

	addis	%r28,%r29,__DTOR_LIST__-1b@ha	/* get &__DTOR_LIST__ */
	addi	%r28,%r28,__DTOR_LIST__-1b@l
	/*
	 * We know the first entry is -1 so skip it.  We use load with update
	 * to advance the pointer along.
	 */
3:
	lwzu	%r0, 4(%r28)			/* get entry */
	cmpwi	%r0, 0				/* end of list? */
	beq	4f				/*    yep. */
	mtctr	%r0				/* setup for indirect branch */
	bctrl					/* and call the routine */
	b	3b				/* do it again */

4:
	/*
	 * if (__deregister_frame_info)
	 *     __deregister_frame_info(&__EH_FRAME_LIST__[0]);
	 */
	lwz	%r0,__deregister_frame_info@got(%r31)
	cmpwi	%r0, 0				/* routine actually exist? */
	beq	5f				/* nope, skip call */

	addis	%r3,%r29,__EH_FRAME_LIST__-1b@ha
	addi	%r3,%r3,__EH_FRAME_LIST__-1b@l
	mtctr	%r0				/* setup for indirect branch */
	bctrl					/* and call the routine */
5:
	lwz	%r28,(4*SZREG)(%r1)		/* restore registers */
	lwz	%r29,(5*SZREG)(%r1)
	lwz	%r30,(6*SZREG)(%r1)
	lwz	%r31,(7*SZREG)(%r1)
	addi	%r1,%r1,8*SZREG			/* adjust stack */
	lwz	%r0,(1*SZREG)(%r1)		/* get return addr */
	mtlr	%r0
	blr					/* and return */

__do_global_ctors_aux:
	mflr	%r7				/* save return address */
	bcl     20,31,1f			/* find out what address */
1:	mflr    %r3				/*   we are at */
	mtlr	%r7				/* restore return address */

	addis   %r4,%r3,__initialized-1b@ha	/* pcrel (hi) */
	lbzu	%r0,__initialized-1b@l(%r4)	/* pcrel (lo) load+update */
	cmpwi	%r0,0				/* have we already done this? */
	bnelr					/* yep, return */

	li	%r0,1
	stb	%r0,0(%r4)			/* remember we've been here */

	stw	%r7,(1*SZREG)(%r1)		/* save LR */
	stwu	%r1,-(8*SZREG)(%r1)		/* alloc our stack frame */
	stw	%r31,(7*SZREG)(%r1)		/* save some registers */
	stw	%r30,(6*SZREG)(%r1)
	stw	%r29,(5*SZREG)(%r1)
	stw	%r28,(4*SZREG)(%r1)

	mr	%r29,%r3			/* move this to a safe place */

	/*
	 * Get a reference to our TOC.
	 */
	addis   %r30,%r29,.LCTOC-1b@ha
	addi    %r30,%r30,.LCTOC-1b@l

	/*
	 * Get a reference to the GOT.
	 */
	addis	%r31,%r29,_GLOBAL_OFFSET_TABLE_-1b@ha	/* pcrel (hi) */
	addi	%r31,%r31,_GLOBAL_OFFSET_TABLE_-1b@l	/* pcrel (lo) */

	/*
	 * if (__register_frame_info)
	 *     __register_frame_info(&__EH_FRAME_LIST__[0], &__dwarf_eh_object)
	 */
	lwz	%r0, __register_frame_info@got(%r31)
	cmpwi	%r0, 0
	beq	2f

	addis	%r3,%r29,__EH_FRAME_LIST__-1b@ha	/* pcrel (hi) */
	addi	%r3,%r3,__EH_FRAME_LIST__-1b@l		/* pcrel (lo) */
	addis	%r4,%r29,__dwarf_eh_object-1b@ha	/* pcrel (hi) */
	addi	%r4,%r4,__dwarf_eh_object-1b@l		/* pcrel (lo) */
	mtctr	%r0				/* setup for indirect branch */
	bctrl					/* and call the routine */

2:
	/*
	 * if (_Jv_RegisterClasses && __JCR_LIST__[0])
	 *     _Jv_RegisterClasses(&__JCR_LIST__[0]);
	 */
	lwz	%r0, _Jv_RegisterClasses@got(%r31)
	cmpwi	%r0, 0
	beq	3f

	mtctr	%r0				/* setup for indirect branch */
	addis   %r3, %r29, __JCR_LIST__-1b@ha	/* pcrel (hi) */
	addi    %r3, %r3, __JCR_LIST__-1b@l	/* pcrel (lo) */
	lwz	%r4, 0(%r3)			/* load first entry */
	cmpwi	%r4, 0				/* is the list empty? */
	bnectrl					/* call the routine if not */
3:

	/*
	 * Get end of list of CTOR list.
	 */
	addis   %r28,%r29,__CTOR_LIST_END__-1b@ha
	addi    %r28,%r28,__CTOR_LIST_END__-1b@l
4:
	lwzu	%r0, -4(%r28)			/* get entry */
	cmpwi	%r0, -1				/* first entry? */
	beq	5f				/*   yes, we're done */
	mtctr	%r0				/* setup for indirect branch */
	bctrl					/* call the routine */
	b	4b				/* do it again */
5:
	lwz	%r28,(4*SZREG)(%r1)		/* restore registers */
	lwz	%r29,(5*SZREG)(%r1)
	lwz	%r30,(6*SZREG)(%r1)
	lwz	%r31,(7*SZREG)(%r1)
	addi	%r1,%r1,8*SZREG			/* adjust stack */
	lwz	%r0,(1*SZREG)(%r1)		/* get return addr */
	mtlr	%r0
	blr					/* and return */

	.section	.init, "ax", @progbits
	bl	__do_global_ctors_aux
	.section	.fini, "ax", @progbits
	bl	__do_global_dtors_aux
