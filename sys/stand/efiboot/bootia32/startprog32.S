/*	$NetBSD: startprog32.S,v 1.2 2017/02/24 12:24:25 nonaka Exp $	*/
/*	NetBSD: startprog.S,v 1.4 2016/12/04 08:21:08 maxv Exp	*/

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
 *   Copyright 1988, 1989, 1990, 1991, 1992
 *    by Intel Corporation, Santa Clara, California.
 *
 *                 All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appears in all
 * copies and that both the copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Intel
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/asm.h>
#include <machine/specialreg.h>

#define	CODE_SEGMENT	0x08
#define	DATA_SEGMENT	0x10

	.align	16
	.globl _C_LABEL(startprog32)
_C_LABEL(startprog32):
	.quad 0

	.globl _C_LABEL(startprog32_size)
_C_LABEL(startprog32_size):
	.long startprog32_end - _C_LABEL(startprog32_start)

	.text
	.p2align 4,,15

/*
 * startprog32(entry,argc,argv,stack,kern_start,kern_load,kern_size,loadaddr)
 */
ENTRY(startprog32_start)
start:
	pushl	%ebp
	movl	%esp, %ebp

	/*
	 * 8(%ebp): kernel entry address
	 * 12(%ebp): argc
	 * 16(%ebp): argv
	 * 20(%ebp): stack address
	 * 24(%ebp): kernel start address
	 * 28(%ebp): loaded kernel address
	 * 32(%ebp): loaded kernel size
	 * 36(%ebp): loaded start address
	 */

	cli

	movl	8(%ebp), %ebx	/* %ebx: entry address */
	movl	36(%ebp), %edx	/* %edx: loaded start address */

	/* Prepare a new stack */
	movl	20(%ebp), %eax	/* stack */
	subl	$4, %eax
	movl	%eax, %edi

	/* Push some number of args onto the stack */
	movl	12(%ebp), %ecx	/* argc */
	movl	%ecx, %eax
	decl	%eax
	shl	$2, %eax
	addl	16(%ebp), %eax	/* ptr to last arg */
	movl	%eax, %esi

	std			/* backwards */
	rep
	movsl			/* copy %ds:(%esi) -> %es:(%edi) */
	cld
	mov	%edi, %esp	/* set new stack pointer */

	/* Copy kernel */
	movl	24(%ebp), %edi	/* dest */
	movl	28(%ebp), %esi	/* src */
	movl	32(%ebp), %ecx	/* size */
#if defined(NO_OVERLAP)
	movl	%ecx, %eax
#else
	movl	%edi, %eax
	subl	%esi, %eax
	cmpl	%ecx, %eax	/* overlapping? */
	movl	%ecx, %eax
	jb	.Lbackwards
#endif
	/* nope, copy forwards. */
	shrl	$2, %ecx	/* copy by words */
	rep
	movsl
	and	$3, %eax	/* any bytes left? */
	jnz	.Ltrailing
	jmp	.Lcopy_done

.Ltrailing:
	cmp	$2, %eax
	jb	1f
	movw	(%esi), %ax
	movw	%ax, (%edi)
	je	.Lcopy_done
	movb	2(%esi), %al
	movb	%al, 2(%edi)
	jmp	.Lcopy_done
1:	movb	(%esi), %al
	movb	%al, (%edi)
	jmp	.Lcopy_done

#if !defined(NO_OVERLAP)
.Lbackwards:
	addl	%ecx, %edi	/* copy backwards. */
	addl	%ecx, %esi
	and	$3, %eax	/* any fractional bytes? */
	jnz	.Lback_align
.Lback_aligned:
	shrl	$2, %ecx
	subl	$4, %esi
	subl	$4, %edi
	std
	rep
	movsl
	cld
	jmp	.Lcopy_done

.Lback_align:
	sub	%eax, %esi
	sub	%eax, %edi
	cmp	$2, %eax
	jb	1f
	je	2f
	movb	2(%esi), %al
	movb	%al, 2(%edi)
2:	movw	(%esi), %ax
	movw	%ax, (%edi)
	jmp	.Lback_aligned
1:	movb	(%esi), %al
	movb	%al, (%edi)
	jmp	.Lback_aligned
#endif
	/* End of copy kernel */
.Lcopy_done:
	cld			/* LynxOS depends on it */

	/* Prepare jump address */
	lea	(start32a - start)(%edx), %eax
	movl	%eax, (start32r - start)(%edx)

	/* Setup GDT */
	lea	(gdt - start)(%edx), %eax
	movl	%eax, (gdtrr - start)(%edx)
	lgdt	(gdtr - start)(%edx)

	/* Jump to set %cs */
	ljmp	*(start32r - start)(%edx)

	.align	4
start32a:
	movl	$DATA_SEGMENT, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Already set new stack pointer */
	movl	%esp, %ebp

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
	movl	%ebx, (start32r - start)(%edx)
	ljmp	*(start32r - start)(%edx)

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
startprog32_end:
