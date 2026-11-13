/*	$NetBSD: startprog.S,v 1.4 2016/12/04 08:21:08 maxv Exp $	*/

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

/*
 * Starts program in protected mode / flat space with given stackframe.
 * Needs global variables flatcodeseg and flatdataseg (gdt offsets).
 */

/*
 * startprog(phyaddr, argc, argv, stack)
 *	start the program on protected mode where phyaddr is the entry point
 */
ENTRY(startprog)
	pushl	%ebp
	movl	%esp,%ebp

	/* Prepare a new stack */
	movl	$flatdataseg,%ebx
	movw	%bx,%es		/* for arg copy */
	movl	20(%ebp),%eax	/* stack */
	subl	$4,%eax
	movl	%eax,%edi

	/* Push some number of args onto the stack */
	movl	12(%ebp),%ecx	/* argc */
	movl	%ecx,%eax
	decl	%eax
	shl	$2,%eax
	addl	16(%ebp),%eax	/* ptr to last arg */
	movl	%eax,%esi

	std			/* backwards */
	rep
	movsl			/* copy %ds:(%esi) -> %es:(%edi) */

	cld

	movl	8(%ebp),%ecx	/* entry */

	/* Set new stack pointer (movsl decd sp 1 more -> dummy return address) */
	movw	%bx,%ss
	movl	%edi,%esp

	/* Push on our entry address */
	movl	$flatcodeseg,%ebx
	pushl	%ebx			/* code segment */
	pushl	%ecx			/* phyaddr */

	/* Convert over the other data segs */
	movl	$flatdataseg,%ebx
	mov	%bx,%ds
	mov	%bx,%es

	/* Jump to phyaddr, with the new code segment */
	lret

