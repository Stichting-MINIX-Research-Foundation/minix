/*	$NetBSD: crtbegin.S,v 1.7 2014/03/16 11:41:30 joerg Exp $	*/
/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
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

#include <vax/asm.h>

RCSID("$NetBSD: crtbegin.S,v 1.7 2014/03/16 11:41:30 joerg Exp $")

	.section	.ctors, "aw", @progbits
	.p2align 2
__CTOR_LIST__:		/* symbol is not used */
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

	.local	__dwarf_eh_object
	.comm	__dwarf_eh_object,32
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

	.hidden __do_global_dtors_aux

_ENTRY(__do_global_dtors_aux, 0x0100)		/* save r8 */
	tstb	__finished			/* done this already? */
	bneq	4f
	movb	$1, __finished			/* mark it as done */

#ifdef SHARED
	/*
	 * if (__cxa_finalize)
	 *	__cxa_finalize(&__dso_handle);
	 */
	movab	__cxa_finalize, %r0
	beql	1f
	pushal	__dso_handle	
	calls	$0, (%r0)
1:
#endif /* SHARED */

	/*
	 * We use load with postincrement to advance the pointer along.
	 * We know the list ends with 0.  If we load one, we must be done.
	 */
	moval	__DTOR_LIST__+4, %r8	/* skip first entry */
2:	movl	(%r8)+, %r0		/* r0 = *r8++; */
	beql	3f
	calls	$0, (%r0)
	brb	2b
3:

#ifdef SHARED
	/*
	 * if (__deregister_frame_info)
	 *     __deregister_frame_info(&__EH_FRAME_LIST__[0]);
	 */
	moval	__deregister_frame_info, %r0
	beql	4f
	pushal	__EH_FRAME_LIST__
	calls	$0, (%r0)
#endif /* SHARED */

4:	ret
END(__do_global_dtors_aux)

	.weak	__deregister_frame_info
	.weak	__cxa_finalize

	.weak	__register_frame_info
	.weak	_Jv_RegisterClasses

	.hidden __do_global_ctors_aux

_ENTRY(__do_global_ctors_aux, 0x0800)
	tstb	__initialized
	bneq	4f
	movb	$1, __initialized

	/*
	 * if (__register_frame_info)
	 *     __register_frame_info(&__EH_FRAME_LIST__[0], &__dwarf_eh_object)
	 */
	movab	__register_frame_info, %r0
	beql	1f

	pushal	__dwarf_eh_object
	pushal	__EH_FRAME_LIST__
	calls	$0, (%r0)
1:

	/*
	 * if (_Jv_RegisterClasses && __JCR_LIST__[0])
	 *     _Jv_RegisterClasses(&__JCR_LIST__[0]);
	 */
	movab	_Jv_RegisterClasses, %r0
	beql	2f

	pushal	__JCR_LIST__
	calls	$0, (%r0)
2:

	/*
	 * Get the end of the CTOR list.  The first entry is -1 so if we
	 * load a negative address, we know we are done.
	 */

	moval	__CTOR_LIST_END__, %r8	/* get end of list */
3:	movl	-(%r8), %r0		/* get function pointer with predec */
	blss	4f			/*  negative?  done. */
	calls	$0, (%r0)		/* call it */
	brb	3b			/* get next one */

4:	ret
END(__do_global_ctors_aux)

	.hidden	_C_LABEL(__CTOR_LIST_END__)

	.section	.init, "ax", @progbits
	calls	$0, __do_global_ctors_aux
	.section	.fini, "ax", @progbits
	calls	$0, __do_global_dtors_aux
