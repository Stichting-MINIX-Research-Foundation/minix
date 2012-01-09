/*	$NetBSD: realprot.S,v 1.10 2010/12/19 17:18:23 jakllsch Exp $	*/

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

/*
 * Loosely based on code from stand/lib/libcrt/bootsect/start_bootsect.S
 */

#include <machine/asm.h>

#define	CR0_PE		1

	.text
	.align  16
gdt:
	.word	0, 0
	.byte	0, 0x00, 0x00, 0

	/* kernel code segment */
	.globl flatcodeseg
flatcodeseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	/* kernel data segment */
	.globl flatdataseg
flatdataseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

	/* boot code segment, base will be patched */
bootcodeseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9e, 0x4f, 0

	/* boot data segment, base will be patched */
bootdataseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x92, 0xcf, 0

	/* 16 bit real mode, base will be patched */
bootrealseg = . - gdt
	.word	0xffff, 0
	.byte	0, 0x9e, 0x00, 0

	/* limits (etc) for data segment in real mode */
bootrealdata = . - gdt
	.word	0xffff, 0
	.byte	0, 0x92, 0x00, 0
gdtlen = . - gdt

	.align	16
gdtarg:
	.word	gdtlen-1		/* limit */
	.long	0			/* physical addr, will be inserted */

toreal:	.word	xreal			/* off:seg address for indirect jump */
ourseg:	.word	0			/* real mode code and data segment */

stkseg:	.word	0			/* real mode stack segment */
stkdif:	.long	0			/* diff. between real and prot sp */

	.global	gdt_fixup
gdt_fixup:
	.code16
	pushl	%eax
	pushl	%edx

	xorl	%eax, %eax
	mov	%cs, %ax
	mov	%ax, ourseg
	/* sort out stuff for %ss != %ds */
	xorl	%edx, %edx
	movw	%ss, %dx
	movw	%dx, stkseg
	subl	%eax, %edx
	shll	$4, %edx
	movl	%edx, stkdif

	/* fix up GDT entries for bootstrap */
	mov	%ax, %dx
	shll	$4, %eax
	shr	$12, %dx

#define FIXUP(gdt_index) \
	movw	%ax, gdt+gdt_index+2; \
	movb	%dl, gdt+gdt_index+4

	FIXUP(bootcodeseg)
	FIXUP(bootrealseg)
	FIXUP(bootdataseg)

	/* fix up GDT pointer */
	addl	$gdt, %eax
	movl	%eax, gdtarg+2

	popl	%edx
	popl	%eax
	ret

/*
 * real_to_prot()
 *
 * Switch CPU to 32bit protected mode to execute C.
 *
 * NB: Call with the 32bit calll instruction so that a 32 bit
 *     return address is pushed.
 *
 * All registers are preserved, %ss:%esp will point to the same
 * place as %ss:%sp did, although the actual value of %esp might
 * be changed.
 *
 * Interrupts are disabled while we are in 32bit mode to save us
 * having to setup a different IDT.  This code is only used during
 * the boot process and it doesn't use any interrupts.
 */
ENTRY(real_to_prot)
	.code16
	pushl	%eax
	cli

	lgdt	%cs:gdtarg		/* Global descriptor table */

	movl	%cr0, %eax
	or	$CR0_PE, %ax
	movl	%eax, %cr0 		/* Enter 'protected mode' */

	ljmp	$bootcodeseg, $1f	/* Jump into a 32bit segment */
1:

	.code32
	/*  Set all the segment registers to map the same area as the code */
	mov	$bootdataseg, %eax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss
	addl	stkdif, %esp		/* Allow for real %ss != %ds */

	popl	%eax
	ret

/*
 * prot_to_real()
 *
 * Switch CPU back to 16bit real mode in order to call system bios functions.
 *
 * All registers are preserved, except that %sp may be changed so that
 * %ss:%sp points to the same memory.
 * Note that %ebp is preserved and will not reference the correct part
 * of the stack.
 *
 * Interrupts are enabled while in real mode.
 *
 * Based on the descripton in section 14.5 of the 80386 Programmer's
 * reference book.
 */
/*
 * EPIA_HACK
 *
 * VIA C3 processors (Eden, Samuel 2) don't seem to correctly switch back to
 * executing 16 bit code after the switch to real mode and subsequent jump.
 *
 * It is speculated that the CPU is prefetching and decoding branch
 * targets and not invalidating this buffer on the long jump.
 * Further investication indicates that the caching of return addresses
 * is most likely the problem.
 *
 * Previous versions just used some extra call/ret and a few NOPs, these
 * only helped a bit, but booting compressed kernels would still fail.
 *
 * Trashing the return address stack (by doing 'call' without matched 'ret')
 * Seems to fix things completely. 1 iteration isn't enough, 16 is plenty.
 */
ENTRY(prot_to_real)
	.code32
	pushl	%eax
#ifdef EPIA_HACK
	push	%ecx
	push	$0x10
	pop	%ecx
1:	call	trash_return_cache
	loop	1b
	pop	%ecx
#endif

	/*
	 * Load the segment registers while still in protected mode.
	 * Otherwise the control bits don't get changed.
	 * The correct base addresses are loaded later.
	 */
	movw    $bootrealdata, %ax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss

	/*
	 * Load %cs with a segment that has the correct attributes for
	 * 16bit operation.
	 */
	ljmp	$bootrealseg, $1f
1:

	.code16
	movl	%cr0, %eax
	and 	$~CR0_PE, %eax
	movl	%eax, %cr0		/* Disable potected mode */

	/* Jump far indirect to load real mode %cs */
	ljmp	*%cs:toreal
xreal:
	/*
	 * CPU is now in real mode, load the other segment registers
	 * with their correct base addresses.
	 */
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	/*
	 * If stack was above 64k, 16bit %ss needs to be different from
	 * 32bit %ss (and the other segment registers).
	 */
	mov	stkseg, %ax
	mov	%ax, %ss
	subl	stkdif, %esp

	/* Check we are returning to an address below 64k */
	push	%bp
	movw	%sp, %bp
	movw	2/*bp*/ + 4/*eax*/ + 2(%bp), %ax	/* high bits ret addr */
	test	%ax, %ax
	jne	1f
	pop	%bp

	sti
	popl	%eax
	retl

1:	movw	$3f, %si
	call	message
	movl	2/*bp*/ + 4/*eax*/(%bp), %eax		/*  return address */
	call	dump_eax
	int	$0x18
2:	sti
	hlt
	jmp	2b
3:	.asciz	"prot_to_real can't return to "

	.global	dump_eax_buff
dump_eax_buff:
	. = . + 16

#ifdef EPIA_HACK
trash_return_cache:
	.code32
	pop	%eax
	jmp	*%eax
#endif

/* vtophys(void *)
 * convert boot time 'linear' address to a physical one
 */

ENTRY(vtophys)
	.code32
	xorl	%eax, %eax
	movw	ourseg, %ax
	shll	$4, %eax
	addl	4(%esp), %eax
	ret
