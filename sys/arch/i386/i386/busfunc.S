/*	$NetBSD: busfunc.S,v 1.9 2013/06/22 05:20:57 uebayasi Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: busfunc.S,v 1.9 2013/06/22 05:20:57 uebayasi Exp $");

#include "assym.h"

/*
 * uint8_t bus_space_read_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset);
 */
ENTRY(bus_space_read_1)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	addl	12(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	je	1f
	movzbl	(%edx), %eax
	ret
1:
	xorl	%eax, %eax
	inb	%dx, %al
	ret
END(bus_space_read_1)

/*
 * uint16_t bus_space_read_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset);
 */
ENTRY(bus_space_read_2)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	addl	12(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	je	1f
	movzwl	(%edx), %eax
	ret
1:
	xorl	%eax, %eax
	inw	%dx, %ax
	ret
END(bus_space_read_2)

/*
 * uint32_t bus_space_read_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset);
 */
ENTRY(bus_space_read_4)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	addl	12(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	je	1f
	movl	(%edx), %eax
	ret
1:
	inl	%dx, %eax
	ret
END(bus_space_read_4)

STRONG_ALIAS(bus_space_read_stream_1,bus_space_read_1)
STRONG_ALIAS(bus_space_read_stream_2,bus_space_read_2)
STRONG_ALIAS(bus_space_read_stream_4,bus_space_read_4)

/*
 * void bus_space_write_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint8_t value);
 */
ENTRY(bus_space_write_1)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	addl	12(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	16(%esp), %eax
	je	1f
	movb	%al, (%edx)
	ret
1:
	outb	%al, %dx
	ret
END(bus_space_write_1)

/*
 * void bus_space_write_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint16_t value);
 */
ENTRY(bus_space_write_2)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	addl	12(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	16(%esp), %eax
	je	1f
	movw	%ax, (%edx)
	ret
1:
	outw	%ax, %dx
	ret
END(bus_space_write_2)

/*
 * void bus_space_write_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *     bus_size_t offset, uint32_t value);
 */
ENTRY(bus_space_write_4)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	addl	12(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	16(%esp), %eax
	je	1f
	movl	%eax, (%edx)
	ret
1:
	outl	%eax, %dx
	ret
END(bus_space_write_4)

STRONG_ALIAS(bus_space_write_stream_1,bus_space_write_1)
STRONG_ALIAS(bus_space_write_stream_2,bus_space_write_2)
STRONG_ALIAS(bus_space_write_stream_4,bus_space_write_4)

/*
 * void bus_space_read_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint8_t *addr, size_t count);
 */
ENTRY(bus_space_read_multi_1)
	pushl	%edi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %edi
	movl	24(%esp), %ecx
	jne	1f
	rep
	insb	%dx, %es:(%edi)
	popl	%edi
	ret
	.align	16
1:
	movb	(%edx), %al
	decl	%ecx
	movb	%al, (%edi)
	leal	1(%edi), %edi
	jnz	1b
	popl	%edi
	ret
END(bus_space_read_multi_1)

/*
 * void bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint16_t *addr, size_t count);
 */
ENTRY(bus_space_read_multi_2)
	pushl	%edi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %edi
	movl	24(%esp), %ecx
	jne	1f
	rep
	insw	%dx, %es:(%edi)
	popl	%edi
	ret
	.align	16
1:
	movw	(%edx), %ax
	decl	%ecx
	movw	%ax, (%edi)
	leal	2(%edi), %edi
	jnz	1b
	popl	%edi
	ret
END(bus_space_read_multi_2)

/*
 * void bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint32_t *addr, size_t count);
 */
ENTRY(bus_space_read_multi_4)
	pushl	%edi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %edi
	movl	24(%esp), %ecx
	jne	1f
	rep
	insl	%dx, %es:(%edi)
	popl	%edi
	ret
	.align	16
1:
	movl	(%edx), %eax
	decl	%ecx
	movl	%eax, (%edi)
	leal	4(%edi), %edi
	jnz	1b
	popl	%edi
	ret
END(bus_space_read_multi_4)

STRONG_ALIAS(bus_space_read_multi_stream_1,bus_space_read_multi_1)
STRONG_ALIAS(bus_space_read_multi_stream_2,bus_space_read_multi_2)
STRONG_ALIAS(bus_space_read_multi_stream_4,bus_space_read_multi_4)

/*
 * void bus_space_write_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint8_t *addr, size_t count);
 */
ENTRY(bus_space_write_multi_1)
	pushl	%esi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %esi
	movl	24(%esp), %ecx
	jne	1f
	rep
	outsb	%ds:(%esi), %dx
	popl	%esi
	ret
	.align	16
1:
	movb	(%esi), %al
	decl	%ecx
	movb	%al, (%edx)
	leal	1(%esi), %esi
	jnz	1b
	popl	%esi
	ret
END(bus_space_write_multi_1)

/*
 * void bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint16_t *addr, size_t count);
 */
ENTRY(bus_space_write_multi_2)
	pushl	%esi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %esi
	movl	24(%esp), %ecx
	jne	1f
	rep
	outsw	%ds:(%esi), %dx
	popl	%esi
	ret
	.align	16
1:
	movw	(%esi), %ax
	decl	%ecx
	movw	%ax, (%edx)
	leal	2(%esi), %esi
	jnz	1b
	popl	%esi
	ret
END(bus_space_write_multi_2)

/*
 * void bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint32_t *addr, size_t count);
 */
ENTRY(bus_space_write_multi_4)
	pushl	%esi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %esi
	movl	24(%esp), %ecx
	jne	1f
	rep
	outsl	%ds:(%esi), %dx
	popl	%esi
	ret
	.align	16
1:
	movl	(%esi), %eax
	decl	%ecx
	movl	%eax, (%edx)
	leal	4(%esi), %esi
	jnz	1b
	popl	%esi
	ret
END(bus_space_write_multi_4)

STRONG_ALIAS(bus_space_write_multi_stream_1,bus_space_write_multi_1)
STRONG_ALIAS(bus_space_write_multi_stream_2,bus_space_write_multi_2)
STRONG_ALIAS(bus_space_write_multi_stream_4,bus_space_write_multi_4)

/*
 * void bus_space_read_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint8_t *addr, size_t count);
 */
ENTRY(bus_space_read_region_1)
	pushl	%edi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %edi
	movl	24(%esp), %ecx
	je	2f
1:
	xchgl	%edx, %esi
	rep
	movsb	%ds:(%esi), %es:(%edi)
	movl	%edx, %esi
	popl	%edi
	ret
2:
	inb	%dx, %al
	incl	%edx
	decl	%ecx
	movb	%al, (%edi)
	leal	1(%edi), %edi
	jnz	2b
	popl	%edi
	ret
END(bus_space_read_region_1)

/*
 * void bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint16_t *addr, size_t count);
 */
ENTRY(bus_space_read_region_2)
	pushl	%edi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %edi
	movl	24(%esp), %ecx
	je	2f
1:
	xchgl	%edx, %esi
	rep
	movsw	%ds:(%esi), %es:(%edi)
	movl	%edx, %esi
	popl	%edi
	ret
2:
	inw	%dx, %ax
	addl	$2, %edx
	decl	%ecx
	movw	%ax, (%edi)
	leal	2(%edi), %edi
	jnz	2b
	popl	%edi
	ret
END(bus_space_read_region_2)

/*
 * void bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint32_t *addr, size_t count);
 */
ENTRY(bus_space_read_region_4)
	pushl	%edi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %edi
	movl	24(%esp), %ecx
	je	2f
1:
	xchgl	%edx, %esi
	rep
	movsl	%ds:(%esi), %es:(%edi)
	movl	%edx, %esi
	popl	%edi
	ret
2:
	inl	%dx, %eax
	addl	$4, %edx
	decl	%ecx
	movl	%eax, (%edi)
	leal	4(%edi), %edi
	jnz	2b
	popl	%edi
	ret
END(bus_space_read_region_4)

STRONG_ALIAS(bus_space_read_region_stream_1,bus_space_read_region_1)
STRONG_ALIAS(bus_space_read_region_stream_2,bus_space_read_region_2)
STRONG_ALIAS(bus_space_read_region_stream_4,bus_space_read_region_4)

/*
 * void bus_space_write_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint8_t *addr, size_t count);
 */
ENTRY(bus_space_write_region_1)
	pushl	%esi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %esi
	movl	24(%esp), %ecx
	je	2f
1:
	xchgl	%edx, %edi
	rep
	movsb	%ds:(%esi), %es:(%edi)
	movl	%edx, %edi
	popl	%esi
	ret
2:
	movb	(%esi), %al
	incl	%esi
	decl	%ecx
	outb	%al, %dx
	leal	1(%edx), %edx
	jnz	2b
	popl	%esi
	ret
END(bus_space_write_region_1)

/*
 * void bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint16_t *addr, size_t count);
 */
ENTRY(bus_space_write_region_2)
	pushl	%esi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %esi
	movl	24(%esp), %ecx
	je	2f
1:
	xchgl	%edx, %edi
	rep
	movsw	%ds:(%esi), %es:(%edi)
	movl	%edx, %edi
	popl	%esi
	ret
2:
	movw	(%esi), %ax
	addl	$2, %esi
	decl	%ecx
	outw	%ax, %dx
	leal	2(%edx), %edx
	jnz	2b
	popl	%esi
	ret
END(bus_space_write_region_2)

/*
 * void bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint32_t *addr, size_t count);
 */
ENTRY(bus_space_write_region_4)
	pushl	%esi
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	addl	16(%esp), %edx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%eax)
	movl	20(%esp), %esi
	movl	24(%esp), %ecx
	je	2f
1:
	xchgl	%edx, %edi
	rep
	movsl	%ds:(%esi), %es:(%edi)
	movl	%edx, %edi
	popl	%esi
	ret
2:
	movl	(%esi), %eax
	addl	$4, %esi
	decl	%ecx
	outl	%eax, %dx
	leal	4(%edx), %edx
	jnz	2b
	popl	%esi
	ret
END(bus_space_write_region_4)

STRONG_ALIAS(bus_space_write_region_stream_1,bus_space_write_region_1)
STRONG_ALIAS(bus_space_write_region_stream_2,bus_space_write_region_2)
STRONG_ALIAS(bus_space_write_region_stream_4,bus_space_write_region_4)
