/*	$NetBSD: crtbegin.S,v 1.2 2010/11/30 18:37:59 joerg Exp $	*/
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

RCSID("$NetBSD: crtbegin.S,v 1.2 2010/11/30 18:37:59 joerg Exp $")

	.section	.ctors, "aw", @progbits
	.align 8
__CTOR_LIST__:
	.quad -1

	.section	.dtors, "aw", @progbits
	.align 8
__DTOR_LIST__:
	.quad -1

	.section	.eh_frame, "a", @progbits
	.align 8
__EH_FRAME_LIST__:

	.section	.jcr, "aw", @progbits
	.align 8
__JCR_LIST__:

	.section	.data.rel, "aw", @progbits
	.align 8
	.type	__dso_handle, @object
	.size	__dso_handle, 8
	.globl	__dso_handle
	.hidden	__dso_handle
__dso_handle:
#ifdef SHARED
	.quad	__dso_handle
#else
	.quad	0
#endif

__dwarf_eh_object:
	.zero	64

__initialized:
	.zero	1
__finished:
	.zero	1

	.text
	.weak	__cxa_finalize
	.weak	__deregister_frame_info
	.weak	__register_frame_info
	.weak	_Jv_RegisterClasses

__do_global_dtors_aux:
	cmpb	$0, __finished(%rip)
	je	1f
	ret
1:
	pushq	%rbx
	movb	$1, __finished(%rip)


#ifdef SHARED
	cmpq	$0, __cxa_finalize@GOTPCREL(%rip)
	je	2f
	movq	__dso_handle(%rip), %rdi
	call	__cxa_finalize@PLT
2:
#endif

	leaq	8+__DTOR_LIST__(%rip), %rbx
3:
	movq	(%rbx), %rax
	testq	%rax, %rax
	je	4f
	call	*%rax
	addq	$8, %rbx
	jmp	3b	
4:

	cmpq	$0, __deregister_frame_info@GOTPCREL(%rip)
	je	5f
	leaq	__EH_FRAME_LIST__(%rip), %rdi
	call	__deregister_frame_info@PLT
5:
	popq	%rbx
	ret


__do_global_ctors_aux:
	cmpb	$0, __initialized(%rip)
	je	1f
	ret
1:
	pushq	%rbx
	movb	$1, __initialized(%rip)

	cmpq	$0, __register_frame_info@GOTPCREL(%rip)
	je	2f
	leaq	__dwarf_eh_object(%rip), %rsi
	leaq	__EH_FRAME_LIST__(%rip), %rdi
	call	__register_frame_info@PLT

2:
	cmpq	$0, _Jv_RegisterClasses@GOTPCREL(%rip)
	je	3f
	leaq	__JCR_LIST__(%rip), %rdi
	cmpq	$0, (%rdi)
	je 	3f
	call	_Jv_RegisterClasses@PLT
3:

	leaq	-8+__CTOR_LIST_END__(%rip), %rbx
4:
	movq	(%rbx), %rax
	cmpq	$-1, %rax
	je	5f
	call	*%rax
	subq	$8, %rbx
	jmp	4b

5:
	popq	%rbx

	ret

	.section	.init, "ax", @progbits
	call	__do_global_ctors_aux
	.section	.fini, "ax", @progbits
	call	__do_global_dtors_aux
