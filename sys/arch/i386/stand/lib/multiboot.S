/*	$NetBSD: multiboot.S,v 1.2.20.1 2019/09/17 19:32:00 martin Exp $	*/

/*
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 * Derived from: NetBSD:sys/arch/i386/stand/lib/startprog.S
 */

/*
 * multiboot(entry, header, stack, magic)
 *	start the program on protected mode, entry is phyaddr.
 */
ENTRY(multiboot)
	pushl	%ebp
	movl	%esp,%ebp

	/* Prepare a new stack */
	movl	$flatdataseg,%eax
	movw	%ax,%es		/* for arg copy */
	movl	16(%ebp),%ebx	/* stack */
	subl	$4,%ebx
	movl	%ebx,%edi

	movl	20(%ebp),%edx	/* magic */
	movl	12(%ebp),%ebx	/* header */
	movl	8(%ebp),%ecx	/* entry */

	/* Set new stack pointer */
	movw	%ax,%ss
	movl	%edi,%esp

	/* Push on our entry address */
	movl	$flatcodeseg,%eax
	pushl	%eax			/* code segment */
	pushl	%ecx			/* phyaddr */

	/* Convert over the other data segs */
	movl	$flatdataseg,%eax
	mov	%ax,%ds
	mov	%ax,%es

	movl	%edx,%eax	/* magic */

	/* Jump to phyaddr, with the new code segment */
	lret

