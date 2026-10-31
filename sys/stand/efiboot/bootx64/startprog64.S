/*	$NetBSD: startprog64.S,v 1.3 2017/02/11 10:23:39 nonaka Exp $	*/
/*	NetBSD: startprog.S,v 1.3 2003/02/01 14:48:18 dsl Exp	*/

/* starts program in protected mode / flat space
 with given stackframe
 needs global variables flatcodeseg and flatdataseg
 (gdt offsets)
  derived from: NetBSD:sys/arch/i386/boot/asm.S
 */

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <machine/asm.h>
#include <machine/specialreg.h>

#define	CODE_SEGMENT	0x08
#define	DATA_SEGMENT	0x10

	.align	16
	.globl _C_LABEL(startprog64)
_C_LABEL(startprog64):
	.quad 0

	.globl _C_LABEL(startprog64_size)
_C_LABEL(startprog64_size):
	.long startprog64_end - _C_LABEL(startprog64_start)

	.text
	.p2align 4,,15

/*
 * startprog64(loadddr,entry,stack,kern_load,kern_start,kern_size)
 */
ENTRY(startprog64_start)
start:
	/*
	 * This function is to call the loaded kernel's start() with
	 * 32bit segment mode from x64 mode.
	 * %rdi: kernel start address
	 * %rsi: loaded kernel address
	 * %rdx: stack address
	 * %rcx: loaded kernel size
	 * %r8 : loaded start address
	 * %r9 : kernel entry address
	 */

	cld		/* LynxOS depends on it */

	cli

	/* Copy kernel */
	mov	%rcx, %r12		/* original kernel size */
	movq	%rdi, %r11		/* for misaligned check */

#if !defined(NO_OVERLAP)
	movq	%rdi, %r13
	subq	%rsi, %r13
#endif

	shrq	$3, %rcx		/* count for copy by words */
	jz	8f			/* j if less than 8 bytes */

	lea	-8(%rdi, %r12), %r14	/* target address of last 8 */
	mov	-8(%rsi, %r12), %r15	/* get last word */
#if !defined(NO_OVERLAP)
	cmpq	%r12, %r13		/* overlapping? */
	jb	10f
#endif

/*
 * Non-overlaping, copy forwards.
 * Newer Intel cpus (Nehalem) will do 16byte read/write transfers
 * if %ecx is more than 76.
 * AMD might do something similar some day.
 */
	and	$7, %r11		/* destination misaligned ? */
	jnz	2f
	rep
	movsq
	mov	%r15, (%r14)		/* write last word */
	jmp	.Lcopy_done

/*
 * Destination misaligned
 * AMD say it is better to align the destination (not the source).
 * This will also re-align copies if the source and dest are both
 * misaligned by the same amount)
 * (I think Nehalem will use its accelerated copy if the source
 * and destination have the same alignment.)
 */
2:
	lea	-9(%r11, %r12), %rcx	/* post re-alignment count */
	neg	%r11			/* now -1 .. -7 */
	mov	(%rsi), %r12		/* get first word */
	mov	%rdi, %r13		/* target for first word */
	lea	8(%rsi, %r11), %rsi
	lea	8(%rdi, %r11), %rdi
	shr	$3, %rcx
	rep
	movsq
	mov	%r12, (%r13)		/* write first word */
	mov	%r15, (%r14)		/* write last word */
	jmp	.Lcopy_done

#if !defined(NO_OVERLAP)
/* Must copy backwards.
 * Reverse copy is probably easy to code faster than 'rep movds'
 * since that requires (IIRC) an extra clock every 3 iterations (AMD).
 * However I don't suppose anything cares that much!
 * The big cost is the std/cld pair - reputedly 50+ cycles on Netburst P4.
 * The copy is aligned with the buffer start (more likely to
 * be a multiple of 8 than the end).
 */
10:
	lea	-8(%rsi, %rcx, 8), %rsi
	lea	-8(%rdi, %rcx, 8), %rdi
	std
	rep
	movsq
	cld
	mov	%r15, (%r14)	/* write last bytes */
	jmp	.Lcopy_done
#endif

/* Less than 8 bytes to copy, copy by bytes */
/* Intel Nehalem optimise 'rep movsb' for <= 7 bytes (9-15 clocks).
 * For longer transfers it is 50+ !
 */
8:	mov	%r12, %rcx

#if !defined(NO_OVERLAP)
	cmpq	%r12, %r13	/* overlapping? */
	jb	81f
#endif

	/* nope, copy forwards. */
	rep
	movsb
	jmp	.Lcopy_done

#if !defined(NO_OVERLAP)
/* Must copy backwards */
81:
	lea	-1(%rsi, %rcx), %rsi
	lea	-1(%rdi, %rcx), %rdi
	std
	rep
	movsb
	cld
#endif
	/* End of copy kernel */
.Lcopy_done:

	mov	%r8, %rdi	/* %rdi: loaded start address */
	mov	%r9, %rsi	/* %rsi: kernel entry address */

	/* Prepare jump address */
	lea	(start32a - start)(%rdi), %rax
	movl	%eax, (start32r - start)(%rdi)

	/* Setup GDT */
	lea	(gdt - start)(%rdi), %rax
	mov	%rax, (gdtrr - start)(%rdi)
	lgdt	(gdtr - start)(%rdi)

	/* Jump to set %cs */
	ljmp	*(start32r - start)(%rdi)

	.align	4
	.code32
start32a:
	movl	$DATA_SEGMENT, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	movl	%edx, %esp

	/* Disable Paging in CR0 */
	movl	%cr0, %eax
	andl	$(~CR0_PG), %eax
	movl	%eax, %cr0

	/* Disable PAE in CR4 */
	movl	%cr4, %eax
	andl	$(~CR4_PAE), %eax
	movl	%eax, %cr4

	jmp	start32b

	.align	4
start32b:
	xor	%eax, %eax
	call	*%esi

	.align	16
start32r:
	.long	0
	.long	CODE_SEGMENT
	.align	16
gdt:
	.long	0, 0
	.byte	0xff, 0xff, 0x00, 0x00, 0x00, 0x9f, 0xcf, 0x00
	.byte	0xff, 0xff, 0x00, 0x00, 0x00, 0x93, 0xcf, 0x00
gdtr:
	.word	gdtr - gdt
gdtrr:
	.quad
start32end:
	/* Space for the stack */
	.align	16
	.space	8192
startprog64_end:
