/*	$NetBSD: crtbegin.S,v 1.1 2010/08/07 18:01:33 joerg Exp $	*/
/*-
 * Copyright (c) 2010 Joerg Sonnenberger <joerg@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

RCSID("$NetBSD: crtbegin.S,v 1.1 2010/08/07 18:01:33 joerg Exp $")

	.section	.ctors, "aw", @progbits
	.align 4
__CTOR_LIST__:
	.long -1

	.section	.dtors, "aw", @progbits
	.align 4
__DTOR_LIST__:
	.long -1

	.section	.eh_frame, "a", @progbits
	.align 4
__EH_FRAME_LIST__:

	.section	.jcr, "aw", @progbits
	.align 4
__JCR_LIST__:

	.section	.data.rel, "aw", @progbits
	.align 4
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

__dwarf_eh_object:
	.zero	32

__initialized:
	.zero	1
__finished:
	.zero	1

	.text
	.weak	__cxa_finalize
	.weak	__deregister_frame_info
	.weak	__register_frame_info
	.weak	_Jv_RegisterClasses

__get_thunk:
	movl	(%esp), %ebx
	ret

__do_global_dtors_aux:
	pushl	%ebx
	pushl	%edi
	subl	$8, %esp
	call	__get_thunk
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx

	cmpb	$0, __finished@GOTOFF(%ebx)
	jne	4f
	movb	$1, __finished@GOTOFF(%ebx)

#ifdef SHARED
	cmpl	$0, __cxa_finalize@GOT(%ebx)
	je	1f
	movl	__dso_handle@GOTOFF(%ebx), %eax
	movl	%eax, (%esp)
	call	__cxa_finalize@PLT
1:
#endif

	leal	4+__DTOR_LIST__@GOTOFF(%ebx), %edi
2:
	movl	(%edi), %eax
	testl	%eax, %eax
	je	3f
	call	*%eax
	addl	$8, %edi
	jmp	2b
3:

	cmpl	$0, __deregister_frame_info@GOT(%ebx)
	je	4f
	leal	__EH_FRAME_LIST__@GOTOFF(%ebx), %eax
	movl	%eax, (%esp)
	call	__deregister_frame_info@PLT
4:
	addl	$8, %esp
	popl	%edi
	popl	%ebx
	ret


__do_global_ctors_aux:
	pushl	%ebx
	pushl	%edi
	subl	$8, %esp
	call	__get_thunk
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx

	cmpb	$0, __initialized@GOTOFF(%ebx)
	jne	4f
	movb	$1, __initialized@GOTOFF(%ebx)

	cmpl	$0, __register_frame_info@GOT(%ebx)
	je	1f
	leal	__dwarf_eh_object@GOTOFF(%ebx), %edi
	movl	%edi, 4(%esp)
	leal	__EH_FRAME_LIST__@GOTOFF(%ebx), %edi
	movl	%edi, (%esp)
	call	__register_frame_info@PLT

1:
	cmpl	$0, _Jv_RegisterClasses@GOT(%ebx)
	je	2f
	leal	__JCR_LIST__@GOTOFF(%ebx), %edi
	movl	(%edi), %edi
	testl	%edi, %edi
	je 	2f
	movl	%edi, (%esp)
	call	_Jv_RegisterClasses@PLT
2:

	leal	-4+__CTOR_LIST_END__@GOTOFF(%ebx), %edi
3:
	movl	(%edi), %eax
	cmpl	$-1, %eax
	je	4f
	call	*%eax
	subl	$4, %edi
	jmp	3b

4:
	addl	$8, %esp
	popl	%edi
	popl	%ebx
	ret

	.section	.init, "ax", @progbits
	call	__do_global_ctors_aux
	.section	.fini, "ax", @progbits
	call	__do_global_dtors_aux
