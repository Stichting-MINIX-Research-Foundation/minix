/*	$NetBSD: rtld_start.S,v 1.12 2012/01/06 10:38:57 skrll Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Fredette.
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
#include <machine/frame.h>

	.import _GLOBAL_OFFSET_TABLE_

ENTRY($rtld_start,HPPA_FRAME_SIZE)

	/* Start stack calling convention. */
	copy	%r3, %r1 
	copy	%sp, %r3
	stw,ma	%r1, HPPA_FRAME_SIZE(%sp)
 
	/*
	 * Save our single argument, the ps_strings pointer. We'll need this
	 * twice later: once to call _rtld, and again to transfer to the
	 * program's entry point.
	 */
	stw     %arg0, HPPA_FRAME_ARG(0)(%r3)

	/*
	 * We can't move to C until we relocate at least the 
	 * Global Offset Table.  Even finding the GOT is tricky
	 * without inadvertently causing the linker to make
	 * relocations for this part of the text segment.
	 */

	bl	L$lpc1, %r19
	depi	0, 31, 2, %r19
L$lpc1:	addil	L'_DYNAMIC - ($PIC_pcrel$0 - 8), %r19
	ldo	R'_DYNAMIC - ($PIC_pcrel$0 - 12)(%r1),%arg0

	/*
	 * Load the absolute address of the beginning of the GOT into %r19, the
	 * shared library linkage table register, leaving it ready-to-use by
	 * the dynamic linker C code.
	 */
	addil	L'_GLOBAL_OFFSET_TABLE_ - ($PIC_pcrel$0 - 16), %r19
	ldo	R'_GLOBAL_OFFSET_TABLE_ - ($PIC_pcrel$0 - 20)(%r1),%r19

	/*
	 * The linker sets the first entry in the GOT to the unrelocated
	 *  address of _DYNAMIC.  Subtract this from the absolute address of
	 * _DYNAMIC to get our relocbase.
	 */
	ldw	0(%r19), %arg1
	sub	%arg0, %arg1, %arg1	; %arg1 = relocbase
	bl	_rtld_relocate_nonplt_self, %rp
	copy	%arg1, %r4		; save for later

	/*
	 * Recover the ps_strings pointer, and take out the
	 * ps_argvstr member.
	 */
	ldw	HPPA_FRAME_ARG(0)(%r3), %arg0	; ps_strings
	ldw	0(%arg0), %arg0		; ps_argvstr member first in struct

	/*
	 * ps_argvstr - 4 would get us a pointer to argc, comparable to the
	 * initial stack pointer on architectures where the stack grows down.
	 * Subtracting an additional eight creates the storage for obj and
	 * cleanup that _rtld needs.
	 */
	ldo	-12(%arg0), %arg0
	stw	%arg0, HPPA_FRAME_ARG(1)(%r3)

	/* Call _rtld, copying relocbase into arg1. */
	bl	_rtld, %rp
	copy	%r4, %arg1		; %arg1 = relocbase

	/* Prepare the arguments for the entry point. */
	ldw	HPPA_FRAME_ARG(1)(%r3), %r1
	ldw	HPPA_FRAME_ARG(0)(%r3), %arg0	; ps_strings
	ldw	0(%r1), %arg1			; cleanup
	ldw	4(%r1), %arg2			; obj

	/* End stack calling convention. */
	ldo	HPPA_FRAME_SIZE(%r3), %sp
	ldw,mb	-HPPA_FRAME_SIZE(%sp), %r3

	/* Go for it. */
	bv	%r0(%ret0)
	copy	%r0, %rp
EXIT($rtld_start)

/*
 * This does our setup for an object's GOT.  %arg0 is the Obj_Entry * for the
 * object, and %arg1 is its GOT pointer.
 */
LEAF_ENTRY(__rtld_setup_hppa_pltgot)

	/*
	 * The second entry of the GOT is reserved for the dynamic linker.  We
	 * put the Obj_Entry * for the object in there.
	 */
	stw	%arg0, 4(%arg1)

	/*
	 * Fill the fixup_func and fixup_ltp members of the PLT stub.  This
	 * stub is inserted by the linker immediately before the GOT.  We use
	 * this stub to enter our binder.
	 */

	bl	L$lpc2, %arg0
	depi	0, 31, 2, %arg0
L$lpc2:	addil	L'_rtld_bind_start - ($PIC_pcrel$0 - 8), %arg0
	ldo	R'_rtld_bind_start - ($PIC_pcrel$0 - 12)(%r1),%arg0

	stw	%arg0, -8(%arg1)
	bv	%r0(%rp)
	stw	%r19, -4(%arg1)
EXIT(__rtld_setup_hppa_pltgot)

/*
 * In order to support lazy binding, this implementation of _rtld_bind_start is
 * very closely tied to the shared-library call stub and the PLT stub, both
 * inserted by the linker.
 */

/*
 * This is a magic branch instruction that is used by GCC's
 * __canonicalize_funcptr_for_compare() function to fixup relocations
 * in order to do function pointer comparisons.
 */

	bl	_rtld_bind, %rp

ENTRY(_rtld_bind_start,HPPA_FRAME_SIZE)

	/* Start stack calling convention.  */
	copy	%r3, %r1 
	copy	%sp, %r3

	stw,ma	%r1, HPPA_FRAME_SIZE(%sp)

	/*
	 * We have to save all calling convention registers that are set by the
	 * caller, because we have to restore them before transferring to the
	 * bound function.  Note that this includes %ret0, %ret1, and %t1.
	 *
	 * %ret0 and %ret1 because they can have meaning on entry to a
	 * function.
	 *
	 * %t1 because it's used by libc to pass on errno values to cerror.
	 */
	stw	%rp, HPPA_FRAME_CRP(%r3)
	stw	%arg0, HPPA_FRAME_ARG(0)(%r3)
	stw	%arg1, HPPA_FRAME_ARG(1)(%r3)
	stw	%arg2, HPPA_FRAME_ARG(2)(%r3)
	stw	%arg3, HPPA_FRAME_ARG(3)(%r3)
	/* 0(%r3) is filled with the saved %r3 above */
	stw	%ret0, 4(%r3)
	stw	%ret1, 8(%r3)
	stw	%t1, 12(%r3)		/* %r22 */

	/*
	 * The linker PLT stub loads %r20 with (GOT - 8) for the object that
	 * needs binding done.  The second entry of the GOT is reserved for the
	 * dynamic linker's use, and we previously stashed the object's
	 * Obj_Entry * there.
	 */
	ldw	12(%r20), %arg0

	/*
	 * The linker shared-library call stub loads %r19 from the shared
	 * linkage member of the PLT entry.  We previously stashed the reloff
	 * of the relocation there.
	 */
	copy	%r19, %arg1

	/*
	 * The linker PLT stub loads %r21 with the fixup_ltp word in itself.
	 * We previously stashed our %r19 value there.
	 */
	bl	_rtld_bind, %rp
	copy	%r21, %r19

	/*
	 * Our hppa version of _rtld_bind returns to us the address of the PLT
	 * entry that it fixed up.  Load the function address and shared
	 * linkage for the newly bound function.
	 */
	ldw	0(%ret0), %r21
	ldw	4(%ret0), %r19

	/* Restore registers saved above. */
	ldw	HPPA_FRAME_CRP(%r3), %rp
	ldw	HPPA_FRAME_ARG(0)(%r3), %arg0
	ldw	HPPA_FRAME_ARG(1)(%r3), %arg1
	ldw	HPPA_FRAME_ARG(2)(%r3), %arg2
	ldw	HPPA_FRAME_ARG(3)(%r3), %arg3
	ldw	4(%r3), %ret0
	ldw	8(%r3), %ret1
	ldw	12(%r3), %t1		/* %r22 */

	/* End stack calling convention. */
	ldo	HPPA_FRAME_SIZE(%r3), %sp
	ldw,mb	-HPPA_FRAME_SIZE(%sp), %r3

	/* Transfer to the function. */
	bv	%r0(%r21)
	nop
EXIT(_rtld_bind_start)
