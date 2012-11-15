/*	$NetBSD: rtld_start.S,v 1.9 2011/12/11 22:07:26 joerg Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

	.text
	.align	16
	.globl	.rtld_start
	.hidden	.rtld_start
	.type	.rtld_start,@function
.rtld_start:
	subq	$16,%rsp		# make room of obj_main and exit proc
	movq	%rsp,%r12		# stack pointer arg to _rtld
	pushq	%rbx			# save ps_strings

	andq	$~15,%rsp		# align rsp, to be restored from r12

	leaq	_GLOBAL_OFFSET_TABLE_(%rip),%rax
	leaq	_DYNAMIC(%rip),%rdi	# &_DYNAMIC
	movq	%rdi,%rbx
	subq	(%rax),%rbx		# relocbase

	movq	%rbx,%rsi
	call	_rtld_relocate_nonplt_self

	movq	%r12,%rdi
	movq	%rbx,%rsi
	call	_rtld			# _rtld(sp, relocbase)

	leaq	-8(%r12),%rsp		# restore saved rsp, including %rbx
	popq	%rbx			# %rbx = ps_strings
	popq	%rdx			# %rdx = cleanup
	popq	%rcx			# %rcx = obj_main
	jmp	*%rax

	.align	4
	.globl	_rtld_bind_start
	.hidden	_rtld_bind_start
	.type	_rtld_bind_start,@function
_rtld_bind_start:	# (obj, reloff)
	# This function is called with a misaligned stack from the PLT
	# due to the additional argument pushed.
	# At this point %rsp % 16 == 8.
	pushfq				# save caller-saved registers
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11

	movq	80(%rsp),%rdi
	movq	88(%rsp),%rsi

	call	_rtld_bind		# call the binder
	movq	%rax,88(%rsp)		# store function in obj

	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%rax
	popfq

	leaq	8(%rsp),%rsp

	ret
