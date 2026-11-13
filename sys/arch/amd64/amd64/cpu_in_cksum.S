/* $NetBSD: cpu_in_cksum.S,v 1.5 2019/11/15 09:50:01 maxv Exp $ */

/*-
 * Copyright (c) 2008 Joerg Sonnenberger <joerg@NetBSD.org>.
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
#ifdef _KERNEL
#include <machine/frameasm.h>
#else
#define KMSAN_INIT_RET(sz)	/* nothing */
#endif
#include "assym.h"

ENTRY(cpu_in_cksum)
	pushq	%rbp
	pushq	%rbx

	/*
	 * During most of the function the following values can
	 * be found in the registers:
	 *
	 * %rdi: The current element in the mbuf chain.
	 * %esi: Remaining bytes to check after the current mbuf.
	 * %ebp: Minimum of %esi at the start of the loop and the
	 *       length of the current mbuf.
	 * %r8:  Overall sum. Carry must be handled on increment.
	 * %r9 and %r10: Partial sums. This are normally modified
	 *       without carry check, see comment in inner loop.
	 * %rbx: Remaining data of current mbuf.
	 * %dh:  Partial sum must be byte swapped before adding up.
	 * %dl:  Current mbuf started at odd position. A word was split.
	 */

	movl	%ecx, %eax
	movl	%edx, %ecx
	movq	%rax, %r8
	xorl	%edx,%edx

	/* All requested bytes checksummed? */
	testl	%esi, %esi
	jz	.Mdone

.Mmbuf_preloop:
	/* No more data to process? */
	testq	%rdi, %rdi
	jz	.Mout_of_mbufs
	movl	M_LEN(%rdi), %ebp
	cmpl	%ebp, %ecx
	jbe	1f
	subl	%ebp, %ecx
	movq	M_NEXT(%rdi), %rdi
	jmp	.Mmbuf_preloop
1:
	subl	%ecx, %ebp
	movq	M_DATA(%rdi), %rbx
	movl	%ecx, %eax
	addq	%rax, %rbx 
	jmp	.Mmbuf_load_data

.Mmbuf_loop:
	/* All requested bytes checksummed? */
	testl	%esi, %esi
	jz	.Mdone

	/* No more data to process? */
	testq	%rdi, %rdi
	jz	.Mout_of_mbufs

	movl	M_LEN(%rdi), %ebp
	movq	M_DATA(%rdi), %rbx
.Mmbuf_load_data:

	/* Skip empty mbufs. */
	testl	%ebp, %ebp
	jz	.Mmbuf_loop_next

	/* If this mbuf is longer than necessary, just truncate it. */
	cmpl	%ebp, %esi
	cmovb	%esi, %ebp
	subl	%ebp, %esi

	xorq	%r9, %r9
	xorq	%r10, %r10

.Mmbuf_align_word:
	/* Already aligned on a word boundary? */
	testb	$1, %bl
	jz	.Mmbuf_align_dword

	/* Invert %dl. */
	testb	%dl, %dl
	setz	%dl

	movzbl	(%rbx), %ecx
	xchgb	%cl, %ch
	addq	%rcx, %r9
	incq	%rbx
	decl	%ebp

.Mmbuf_align_dword:
	/*
	 * If the current position is equivalent to an odd index,
	 * byte swap the partial sums at the end to compensate.
	 */
	movb	%dl, %dh

	/*
	 * If the data is not already aligned at a dword boundary,
	 * just add the first word to one of the partial sums.
	 */
	testb	$2, %bl
	jz	.Mmbuf_inner_loop
	cmpl	$2, %ebp
	jb	.Mmbuf_trailing_bytes
	movzwl	(%rbx), %ecx
	addq	%rcx, %r9
	leaq	2(%rbx), %rbx
	leal	-2(%ebp), %ebp

.Mmbuf_inner_loop:
	.align	16
	/*
	 * Inner loop is unrolled to handle 32 byte at a time.
	 * Dwords are summed up in %r9 and %10 without checking
	 * for overflow. This exploits two adders and the order
	 * constraint on flags.
	 *
	 * After the summing up, %r9 and %r10 are merged and
	 * the sum is test for having either of the two highest
	 * bits set. If that is the case, the partial sum is added
	 * to the overall sum and both registers are zeroed.
	 */
	cmpl	$32, %ebp
	jb	.Mmbuf_trailing_owords
	movl	0(%rbx), %ecx
	movl	4(%rbx), %eax
	addq	%rcx, %r9
	addq	%rax, %r10

	movl	8(%rbx), %ecx
	movl	12(%rbx), %eax
	addq	%rcx, %r9
	addq	%rax, %r10

	movl	16(%rbx), %ecx
	movl	20(%rbx), %eax
	addq	%rcx, %r9
	addq	%rax, %r10

	movl	24(%rbx), %ecx
	movl	28(%rbx), %eax
	addq	%rcx, %r9
	addq	%rax, %r10

	leaq	32(%rbx), %rbx
	leal	-32(%ebp), %ebp

	addq	%r9, %r10
	movq	%r10, %rax
	shrq	$62, %rax
	xorq	%r9, %r9
	testb	%al, %al
	jz	.Mmbuf_inner_loop

	testb	%dh, %dh
	jz	1f
	rolq	$8, %r10
1:
	addq	%r10, %r8
	adcq	$0, %r8
	xorq	%r10, %r10

	jmp	.Mmbuf_inner_loop

	/*
	 * One more check for 16, 8, 4, 2 and 1 remaining
	 * byte in the mbuf...
	 *
	 * No more overflow checks needed here.
	 */
.Mmbuf_trailing_owords:
	testw	$16, %bp
	jz	.Mmbuf_trailing_qwords
	movl	0(%rbx), %ecx
	movl	4(%rbx), %eax
	addq	%rcx, %r9
	addq	%rax, %r10

	movl	8(%rbx), %ecx
	movl	12(%rbx), %eax
	addq	%rcx, %r9
	addq	%rax, %r10

	leaq	16(%rbx), %rbx

.Mmbuf_trailing_qwords:
	testw	$8, %bp
	jz	.Mmbuf_trailing_dwords
	movl	0(%rbx), %ecx
	movl	4(%rbx), %eax
	addq	%rcx, %r9
	addq	%rax, %r10

	leaq	8(%rbx), %rbx

.Mmbuf_trailing_dwords:
	testw	$4, %bp
	jz	.Mmbuf_trailing_words
	movl	(%rbx), %ecx
	addq	%rcx, %r9
	leaq	4(%rbx), %rbx

.Mmbuf_trailing_words:
	testw	$2, %bp
	jz	.Mmbuf_trailing_bytes
	movzwl	(%rbx), %ecx
	addq	%rcx, %r9
	leaq	2(%rbx), %rbx

.Mmbuf_trailing_bytes:
	testw	$1, %bp
	jz	.Mbyte_swap
	movzbl	(%rbx), %ecx
	addq	%rcx, %r9
	/* Invert %dl as this is a split in a word. */
	testb	%dl, %dl
	setz	%dl

.Mbyte_swap:
	/* Byte swap by 8 bit rotate. */
	testb	%dh, %dh
	jz	1f
	rolq	$8, %r9
	rolq	$8, %r10
1:
	addq	%r10, %r8
	adcq	%r9, %r8
	adcq	$0, %r8

.Mmbuf_loop_next:
	movq	M_NEXT(%rdi), %rdi
	jmp	.Mmbuf_loop

.Mdone:
	/*
	 * Reduce 64 bit overall sum into 16 bit sum and
	 * return the complement.
	 */
	movq	%r8, %rax
	movq	%r8, %rbx
	shrq	$32, %rax
	addl	%eax, %ebx
	adcl	$0, %ebx
	movzwl	%bx, %eax
	shrl	$16, %ebx
	addw	%ax, %bx
	adcw	$0, %bx
	movw	%bx, %ax
	notw	%ax

.Mreturn:
	popq	%rbx
	popq	%rbp
	KMSAN_INIT_RET(4)
	ret

.Mout_of_mbufs:
#ifdef __PIC__
	leaq	.Mout_of_mbufs_msg(%rip), %rdi
#else
	movq	$.Mout_of_mbufs_msg, %rdi
#endif
	movl	$0, %eax
	call	PIC_PLT(_C_LABEL(printf))
	jmp	.Mreturn
END(cpu_in_cksum)

	.section	.rodata
	.type		.Mout_of_mbufs_msg, @object
.Mout_of_mbufs_msg:
	.string		"in_cksum: out of data\n"
END(.Mout_of_mbufs_msg)
