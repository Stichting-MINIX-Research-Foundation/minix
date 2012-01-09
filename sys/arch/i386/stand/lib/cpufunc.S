/*	$NetBSD: cpufunc.S,v 1.4 2011/06/08 16:03:42 joerg Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

NENTRY(x86_read_psl)
	pushfl
	popl	%eax
	ret

NENTRY(x86_write_psl)
	movl	4(%esp), %eax
	pushl	%eax
	popfl
	ret

NENTRY(x86_disable_intr)
	cli
	ret

NENTRY(x86_enable_intr)
	sti
	ret

NENTRY(inb)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inb	%dx, %al
	ret

NENTRY(insb)
	pushl	%edi
	movl	8(%esp), %edx
	movl	12(%esp), %edi
	movl	16(%esp), %ecx
	cld
	rep
	insb
	popl	%edi
	ret

NENTRY(inw)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inw	%dx, %ax
	ret

NENTRY(insw)
	pushl	%edi
	movl	8(%esp), %edx
	movl	12(%esp), %edi
	movl	16(%esp), %ecx
	cld
	rep
	insw
	popl	%edi
	ret

NENTRY(inl)
	movl	4(%esp), %edx
	inl	%dx, %eax
	ret

NENTRY(insl)
	pushl	%edi
	movl	8(%esp), %edx
	movl	12(%esp), %edi
	movl	16(%esp), %ecx
	cld
	rep
	insl
	popl	%edi
	ret

NENTRY(outb)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	outb	%al, %dx
	ret

NENTRY(outsb)
	pushl	%esi
	movl	8(%esp), %edx
	movl	12(%esp), %esi
	movl	16(%esp), %ecx
	cld
	rep
	outsb
	popl	%esi
	ret

NENTRY(outw)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	outw	%ax, %dx
	ret

NENTRY(outsw)
	pushl	%esi
	movl	8(%esp), %edx
	movl	12(%esp), %esi
	movl	16(%esp), %ecx
	cld
	rep
	outsw
	popl	%esi
	ret

NENTRY(outl)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	outl	%eax, %dx
	ret

NENTRY(outsl)
	pushl	%esi
	movl	8(%esp), %edx
	movl	12(%esp), %esi
	movl	16(%esp), %ecx
	cld
	rep
	outsl
	popl	%esi
	ret
