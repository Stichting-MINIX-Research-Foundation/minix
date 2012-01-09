/*	$NetBSD: biosboot.S,v 1.8 2011/01/05 23:13:01 jakllsch Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
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
#include <sys/bootblock.h>

/*
 * Code linked to 0x1000:0 and (usually) read from /boot by bootxx code
 *
 * On entry:
 * 	%dl			BIOS drive number
 *	%ecx:%ebx		Sector number of NetBSD partition
 *	%ds:%si			Boot parameter block (patched by installboot)
 *	%cs			0x1000
 *	%ds, %es, %ss		All zero
 *	%sp			near 0xfffc
 */

	.text
	.code16
ENTRY(boot_start)
	jmp	boot_start_1
	.balign	4
ENTRY(boot_magic)
	.long	X86_BOOT_MAGIC_2	/* checked for by bootxx code */
ENTRY(boot_params)
	.long	boot_start_1 - boot_params
#include <boot_params.S>
	. = boot_start + 0x80		/* space for patchable variables */
boot_start_1:

#if 0
	/* Allow for boot_start not being %cs:0 */
	call	2f
2:	pop	%cx
	sub	$2b, %cx		/* %ax is offset */
	test	$0xf, %cx		/* check code seg aligned */
	jz	3f
	lret				/* not playing if not */
3:	mov	%cs, %ax
	shr	$4, %cx
	add	%cx, %ax		/* segment staring at boot_start */
	push	%ax
	push	$4f
	lret
4:
#endif

	mov	%cs, %ax
	mov	%ax, %es

	movl	%ecx, %ebp		/* move LBA out of the way */

	/* Grab boot_params patched into bootxx by installboot */
	cmpl	$X86_BOOT_MAGIC_1,-4(%si)	/* sanity check ptr */
	jne	2f
	mov	$boot_params, %di
	movl	(%si),%ecx
	cmp	$boot_start_1 - boot_params, %cx
	jbe	1f
	mov	$boot_start_1 - boot_params, %cx
1:	cld
	rep
	movsb
2:

	mov	%ax, %ds
	movl	$_end, %eax		/* top of bss */
	shr	$4, %eax		/* as a segment */
	add	$0x1001, %ax		/* and + 64k */
	mov	%ax, %ss		/* for stack */
	mov	$0xfffc, %sp		/* %sp at top of it */

	call	gdt_fixup

	calll	real_to_prot
	.code32

	movl	$_end, %ecx		/* zero bss */
	movl	$__bss_start, %edi
	subl	%edi, %ecx
	shr	$2, %ecx		/* _end and __bss_start are aligned */
	xor	%eax, %eax
	rep
	stosl

	testb	$X86_BP_FLAGS_LBA64VALID, boot_params+4
	jnz	1f
	xorl	%ebp, %ebp		/* high part of LBA is not valid */
1:

	movzbl	%dl, %edx
	push	%ebp			/* high 32 bits of first sector */
	push	%ebx			/* first sector of bios partition */
	push	%edx			/* bios disk */
	call	_C_LABEL(boot2)		/* C bootstrap code */
	addl	$12, %esp
	call	prot_to_real
	.code16

boot_fail:
	push	%ax
	movw	$1f, %si
	call	message
	pop	%si
	call	message
	jmp	loopstop
1:	.asciz	"Boot2 failed: "

ENTRY(_rtt)
	.code32
	call	prot_to_real
	.code16
loopstop:
	movb	$0x86, %ah		/* delay for about a second */
	movw	$16, %cx
	int	$0x15
	int	$0x18			/* might be a boot fail entry */
1:	sti
	hlt
	jmp	1b
