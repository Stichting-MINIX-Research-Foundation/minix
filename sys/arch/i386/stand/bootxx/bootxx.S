/*	$NetBSD: bootxx.S,v 1.10 2011/01/06 01:08:48 jakllsch Exp $	*/

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
 * Code linked to 0xa00 and copied to sectors 2+ of the netbsd boot
 * partition by MI /usr/sbin/installboot.
 * Read into memory by code in pbr.S
 *
 * On entry:
 * 	%dl			BIOS drive number
 *	%edi:%esi		Sector number of NetBSD partition
 *	%cs, %ds, %es, %ss	All zero
 *	%sp			near 0xfffc
 */
	.text
	.code16
ENTRY(bootxx)
	jmp	1f
	.balign	4
ENTRY(bootxx_magic)
	.long	X86_BOOT_MAGIC_1	/* checked by installboot & pbr code */
boot_params:				/* space for patchable variables */
	.long	1f - boot_params	/* length of this data area */
#include <boot_params.S>
	. = bootxx + 0x80		/* Space for patching unknown params */

1:	call	gdt_fixup

	calll	real_to_prot
	.code32

	push	%edi
	movl	$_end, %ecx		/* zero bss */
	movl	$__bss_start, %edi
	subl	%edi, %ecx
	shr	$2, %ecx		/* _end and __bss_start are aligned */
	xor	%eax, %eax
	rep
	stosl
	pop	%edi

	movzbl	%dl, %edx
	push	%edi			/* save args for secondary bootstrap */
	push	%esi
	movl	%esp, %esi		/* address of sector number */
	push	%edx
	push	%esi			/* args for boot1 */
	push	%edx
	call	_C_LABEL(boot1)		/* C code to load /boot */
	add	$8, %esp
	call	prot_to_real
	.code16

	test	%ax, %ax
	jnz	boot_fail

	pop	%edx			/* bios disk number */
	pop	%ebx			/* expected partition start sector */
	pop	%ecx
	movl	$boot_params, %esi
	orb	$X86_BP_FLAGS_LBA64VALID, 4(%esi)
	lcall	$SECONDARY_LOAD_ADDRESS/16, $0

boot_fail:
	push	%ax			/* error string from boot1 */
	movw	errno, %ax
	aam				/* largest errno is < 100 */
	addw	$('0' << 8) | '0', %ax	/* to ascii */
	rorw	$8, %ax
	cmpb	$'0', %al		/* supress leading zero */
	jne	10f
	movb	$' ', %al
10:	movw	%ax, 12f
	movw	$11f, %si
	call	message			/* output boot failed message */
	pop	%si
	call	message			/* and text from boot1 */
	jmp	loopstop
11:	.ascii	"Boot failed (errno "
12:	.asciz	"xx): "

ENTRY(_rtt)
	.code32
	call	prot_to_real
	.code16
loopstop:
	movb	0x86, %ah		/* delay for about a second */
	movw	$16, %cx
	int	$0x15
	int	$0x18			/* might be a boot fail entry */
1:	sti				/* if not loopstop */
	hlt
	jmp	1b

	/*
	 * Vector the fs calls through here so we can support multiple
	 * file system types with one copy of the library code and
	 * multiple copies of this file.
	 */
	.global	xxfs_open, xxfs_close, xxfs_read, xxfs_stat
	.code32
xxfs_open:	jmp	XXfs_open
xxfs_close:	jmp	XXfs_close
xxfs_read:	jmp	XXfs_read
xxfs_stat:	jmp	XXfs_stat
