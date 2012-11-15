/*	$NetBSD: memcpy.S,v 1.1 2011/06/16 16:39:14 joerg Exp $	*/
/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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
	RCSID("$NetBSD: memcpy.S,v 1.1 2011/06/16 16:39:14 joerg Exp $")

#  ifdef BCOPY
ENTRY(bcopy)
	pushl	%esi
	pushl	%edi
	movl	12(%esp), %esi
	movl	16(%esp), %edi
	jmp	__memmove_start
#  endif
#  if defined(MEMMOVE)
ENTRY(memcpy)
ENTRY(memmove)
	pushl	%esi
	pushl	%edi
	movl	12(%esp), %edi
	movl	16(%esp), %esi
	.globl __memmove_start
__memmove_start:
	movl	20(%esp), %ecx
	pushl	%edi
	testl %ecx, %ecx
	jz 2f
	cmpl	%esi, %edi
	jb 1f
	addl %ecx, %esi
	decl %esi
	addl %ecx, %edi
	decl %edi
	std
1:
	rep
	movsb
2:
	cld
	popl	%eax
	popl	%edi
	popl	%esi
	ret
#  endif /* MEMMOVE */
