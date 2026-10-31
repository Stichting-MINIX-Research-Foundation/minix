/*	$NetBSD: busfunc.S,v 1.12 2019/11/14 16:23:52 maxv Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
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
#include <machine/frameasm.h>

#include "assym.h"

.Ldopanic:
	movq	$.Lpstr, %rdi
	call	_C_LABEL(panic)
.Lpstr:	.string	"bus_space: 8-byte access to I/O space\0"

/*
 * uint8_t bus_space_read_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset);
 */
ENTRY(bus_space_read_1)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	1f
	movzbl	(%rdx), %eax
	KMSAN_INIT_RET(1)
	ret
1:
	xorl	%eax, %eax
	inb	%dx, %al
	KMSAN_INIT_RET(1)
	ret
END(bus_space_read_1)

/*
 * uint16_t bus_space_read_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset);
 */
ENTRY(bus_space_read_2)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	1f
	movzwl	(%rdx), %eax
	KMSAN_INIT_RET(2)
	ret
1:
	xorl	%eax, %eax
	inw	%dx, %ax
	KMSAN_INIT_RET(2)
	ret
END(bus_space_read_2)

/*
 * uint32_t bus_space_read_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset);
 */
ENTRY(bus_space_read_4)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	1f
	movl	(%rdx), %eax
	KMSAN_INIT_RET(4)
	ret
1:
	inl	%dx, %eax
	KMSAN_INIT_RET(4)
	ret
END(bus_space_read_4)

/*
 * uint64_t bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset);
 */
ENTRY(bus_space_read_8)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	.Ldopanic
	movq	(%rdx), %rax
	KMSAN_INIT_RET(8)
	ret
END(bus_space_read_8)

STRONG_ALIAS(bus_space_read_stream_1,bus_space_read_1)
STRONG_ALIAS(bus_space_read_stream_2,bus_space_read_2)
STRONG_ALIAS(bus_space_read_stream_4,bus_space_read_4)
STRONG_ALIAS(bus_space_read_stream_8,bus_space_read_8)

/*
 * void bus_space_write_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint8_t value);
 */
ENTRY(bus_space_write_1)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	1f
	movb	%cl, (%rdx)
	ret
1:
	movl	%ecx, %eax
	outb	%al, %dx
	ret
END(bus_space_write_1)

/*
 * void bus_space_write_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint16_t value);
 */
ENTRY(bus_space_write_2)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	1f
	movw	%cx, (%rdx)
	ret
1:
	movl	%ecx, %eax
	outw	%ax, %dx
	ret
END(bus_space_write_2)

/*
 * void bus_space_write_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *     bus_size_t offset, uint32_t value);
 */
ENTRY(bus_space_write_4)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	1f
	movl	%ecx, (%rdx)
	ret
1:
	movl	%ecx, %eax
	outl	%eax, %dx
	ret
END(bus_space_write_4)

/*
 * void bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t bsh,
 *     bus_size_t offset, uint64_t value);
 */
ENTRY(bus_space_write_8)
	addq	%rsi, %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	.Ldopanic
	movq	%rcx, (%rdx)
	ret
END(bus_space_write_8)

STRONG_ALIAS(bus_space_write_stream_1,bus_space_write_1)
STRONG_ALIAS(bus_space_write_stream_2,bus_space_write_2)
STRONG_ALIAS(bus_space_write_stream_4,bus_space_write_4)
STRONG_ALIAS(bus_space_write_stream_8,bus_space_write_8)

/*
 * void bus_space_read_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint8_t *addr, size_t count);
 */
ENTRY(bus_space_read_multi_1)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	jne	1f
	movq	%rcx, %rdi
	movq	%r8, %rcx
	rep
	insb	%dx, %es:(%rdi)
	ret
	.align	16
1:
	movb	(%rdx), %al
	movb	%al, (%rcx)
	leaq	1(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_read_multi_1)

/*
 * void bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint16_t *addr, size_t count);
 */
ENTRY(bus_space_read_multi_2)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	jne	1f
	movq	%rcx, %rdi
	movq	%r8, %rcx
	rep
	insw	%dx, %es:(%rdi)
	ret
	.align	16
1:
	movw	(%rdx), %ax
	movw	%ax, (%rcx)
	leaq	2(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_read_multi_2)

/*
 * void bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint32_t *addr, size_t count);
 */
ENTRY(bus_space_read_multi_4)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	jne	1f
	movq	%rcx, %rdi
	movq	%r8, %rcx
	rep
	insl	%dx, %es:(%rdi)
	ret
	.align	16
1:
	movl	(%rdx), %eax
	movl	%eax, (%rcx)
	leaq	4(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_read_multi_4)

/*
 * void bus_space_read_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint64_t *addr, size_t count);
 */
ENTRY(bus_space_read_multi_8)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	.Ldopanic
	.align	16
1:
	movq	(%rdx), %rax
	movq	%rax, (%rcx)
	leaq	8(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_read_multi_8)

STRONG_ALIAS(bus_space_read_multi_stream_1,bus_space_read_multi_1)
STRONG_ALIAS(bus_space_read_multi_stream_2,bus_space_read_multi_2)
STRONG_ALIAS(bus_space_read_multi_stream_4,bus_space_read_multi_4)
STRONG_ALIAS(bus_space_read_multi_stream_8,bus_space_read_multi_8)

/*
 * void bus_space_write_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint8_t *addr, size_t count);
 */
ENTRY(bus_space_write_multi_1)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	jne	1f
	movq	%rcx, %rsi
	movq	%r8, %rcx
	rep
	outsb	%ds:(%rsi), %dx
	ret
	.align	16
1:
	movb	(%rcx), %al
	movb	%al, (%rdx)
	leaq	1(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_write_multi_1)

/*
 * void bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint16_t *addr, size_t count);
 */
ENTRY(bus_space_write_multi_2)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	jne	1f
	movq	%rcx, %rsi
	movq	%r8, %rcx
	rep
	outsw	%ds:(%rsi), %dx
	ret
	.align	16
1:
	movw	(%rcx), %ax
	movw	%ax, (%rdx)
	leaq	2(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_write_multi_2)

/*
 * void bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint32_t *addr, size_t count);
 */
ENTRY(bus_space_write_multi_4)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	jne	1f
	movq	%rcx, %rsi
	movq	%r8, %rcx
	rep
	outsl	%ds:(%rsi), %dx
	ret
	.align	16
1:
	movl	(%rcx), %eax
	movl	%eax, (%rdx)
	leaq	4(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_write_multi_4)

/*
 * void bus_space_write_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint64_t *addr, size_t count);
 */
ENTRY(bus_space_write_multi_8)
	leaq	(%rsi,%rdx,1), %rdx
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	.Ldopanic
	.align	16
1:
	movq	(%rcx), %rax
	movq	%rax, (%rdx)
	leaq	8(%rcx), %rcx
	decq	%r8
	jnz	1b
	ret
END(bus_space_write_multi_8)

STRONG_ALIAS(bus_space_write_multi_stream_1,bus_space_write_multi_1)
STRONG_ALIAS(bus_space_write_multi_stream_2,bus_space_write_multi_2)
STRONG_ALIAS(bus_space_write_multi_stream_4,bus_space_write_multi_4)
STRONG_ALIAS(bus_space_write_multi_stream_8,bus_space_write_multi_8)

/*
 * void bus_space_read_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint8_t *addr, size_t count);
 */
ENTRY(bus_space_read_region_1)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	2f
	addq	%rdx, %rsi
	movq	%rcx, %rdi
	movq	%r8, %rcx
	rep
	movsb	%ds:(%rsi), %es:(%rdi)
	ret
2:
	addl	%esi, %edx
3:
	inb	%dx, %al
	incl	%edx
	decq	%r8
	movb	%al, (%rcx)
	leaq	1(%rcx), %rcx
	jnz	3b
	ret
END(bus_space_read_region_1)

/*
 * void bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint16_t *addr, size_t count);
 */
ENTRY(bus_space_read_region_2)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	2f
	addq	%rdx, %rsi
	movq	%rcx, %rdi
	movq	%r8, %rcx
	rep
	movsw	%ds:(%rsi), %es:(%rdi)
	ret
2:
	addl	%esi, %edx
3:
	inw	%dx, %ax
	addl	$2, %edx
	decq	%r8
	movw	%ax, (%rcx)
	leaq	2(%rcx), %rcx
	jnz	3b
	ret
END(bus_space_read_region_2)

/*
 * void bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint32_t *addr, size_t count);
 */
ENTRY(bus_space_read_region_4)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	2f
	addq	%rdx, %rsi
	movq	%rcx, %rdi
	movq	%r8, %rcx
	rep
	movsl	%ds:(%rsi), %es:(%rdi)
	ret
2:
	addl	%esi, %edx
3:
	inl	%dx, %eax
	addl	$4, %edx
	decq	%r8
	movl	%eax, (%rcx)
	leaq	4(%rcx), %rcx
	jnz	3b
	ret
END(bus_space_read_region_4)

/*
 * void bus_space_read_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, uint64_t *addr, size_t count);
 */
ENTRY(bus_space_read_region_8)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	.Ldopanic
	addq	%rdx, %rsi
	movq	%rcx, %rdi
	movq	%r8, %rcx
	rep
	movsq	%ds:(%rsi), %es:(%rdi)
	ret
END(bus_space_read_region_8)

STRONG_ALIAS(bus_space_read_region_stream_1,bus_space_read_region_1)
STRONG_ALIAS(bus_space_read_region_stream_2,bus_space_read_region_2)
STRONG_ALIAS(bus_space_read_region_stream_4,bus_space_read_region_4)
STRONG_ALIAS(bus_space_read_region_stream_8,bus_space_read_region_8)

/*
 * void bus_space_write_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint8_t *addr, size_t count);
 */
ENTRY(bus_space_write_region_1)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	2f
	leaq	(%rdx,%rsi,1), %rdi
	movq	%rcx, %rsi
	movq	%r8, %rcx
	rep
	movsb	%ds:(%rsi), %es:(%rdi)
	ret
2:
	addl	%esi, %edx
3:
	movb	(%rcx), %al
	incq	%rcx
	decq	%r8
	outb	%al, %dx
	leaq	1(%rdx), %rdx
	jnz	3b
	ret
END(bus_space_write_region_1)

/*
 * void bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint16_t *addr, size_t count);
 */
ENTRY(bus_space_write_region_2)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	2f
	leaq	(%rdx,%rsi,1), %rdi
	movq	%rcx, %rsi
	movq	%r8, %rcx
	rep
	movsw	%ds:(%rsi), %es:(%rdi)
	ret
2:
	addl	%esi, %edx
3:
	movw	(%rcx), %ax
	addq	$2, %rcx
	decq	%r8
	outw	%ax, %dx
	leaq	2(%rdx), %rdx
	jnz	3b
	ret
END(bus_space_write_region_2)

/*
 * void bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint32_t *addr, size_t count);
 */
ENTRY(bus_space_write_region_4)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	2f
	leaq	(%rdx,%rsi,1), %rdi
	movq	%rcx, %rsi
	movq	%r8, %rcx
	rep
	movsl	%ds:(%rsi), %es:(%rdi)
	ret
2:
	addl	%esi, %edx
3:
	movl	(%rcx), %eax
	addq	$4, %rcx
	decq	%r8
	outl	%eax, %dx
	leaq	4(%rdx), %rdx
	jnz	3b
	ret
END(bus_space_write_region_4)

/*
 * void bus_space_write_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
 *    bus_size_t offset, const uint64_t *addr, size_t count);
 */
ENTRY(bus_space_write_region_8)
	cmpl	$X86_BUS_SPACE_IO, BST_TYPE(%rdi)
	je	.Ldopanic
	leaq	(%rdx,%rsi,1), %rdi
	movq	%rcx, %rsi
	movq	%r8, %rcx
	rep
	movsq	%ds:(%rsi), %es:(%rdi)
	ret
END(bus_space_write_region_8)

STRONG_ALIAS(bus_space_write_region_stream_1,bus_space_write_region_1)
STRONG_ALIAS(bus_space_write_region_stream_2,bus_space_write_region_2)
STRONG_ALIAS(bus_space_write_region_stream_4,bus_space_write_region_4)
STRONG_ALIAS(bus_space_write_region_stream_8,bus_space_write_region_8)
