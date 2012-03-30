/*	$NetBSD: rtld_start.S,v 1.11 2010/12/05 00:56:07 joerg Exp $	*/

/*-
 * Copyright (c) 1998, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and by Charles M. Hannum.
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

	.text
	.align	4
	.globl	.rtld_start
	.hidden	.rtld_start
	.type	.rtld_start,@function
.rtld_start:
	subl	$8,%esp			# make room of obj_main and exit proc
	pushl	%ebx			# save ps_strings

	call	1f
1:
	popl	%edx
	leal	_DYNAMIC-1b(%edx),%ecx	# &_DYNAMIC
	movl	%ecx,%ebx
	subl	_GLOBAL_OFFSET_TABLE_-1b(%edx),%ebx

	pushl	%ebx			# relocbase
	pushl	%ecx			# &_DYNAMIC
	call	_rtld_relocate_nonplt_self

	leal	12(%esp),%eax		# &cleanup
	pushl	%ebx			# relocbase
	pushl	%eax			# sp
	call	_rtld			# _rtld(sp, relocbase)

	addl	$16,%esp		# pop args

	popl	%ebx			# %ebx = ps_strings
	popl	%edx			# %edx = cleanup
	popl	%ecx			# %ecx = obj_main
	jmp	*%eax

	.align	4
	.globl	_rtld_bind_start
	.hidden	_rtld_bind_start
	.type	_rtld_bind_start,@function
_rtld_bind_start:	# (obj, reloff)
	pushf				# save registers
	pushl	%eax
	pushl	%ecx
	pushl	%edx

	pushl	20(%esp)		# Copy of reloff
	pushl	20(%esp)		# Copy of obj
	call	_rtld_bind		# Call the binder
	addl	$8,%esp			# pop binder args
	movl	%eax,20(%esp)		# Store function to be called in obj

	popl	%edx
	popl	%ecx
	popl	%eax
	popf

	leal	4(%esp),%esp		# Discard reloff, do not change eflags
	ret
