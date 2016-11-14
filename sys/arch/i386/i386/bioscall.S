/*	$NetBSD: bioscall.S,v 1.9 2010/07/24 00:45:54 jym Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John Kohl.
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
__KERNEL_RCSID(0, "$NetBSD: bioscall.S,v 1.9 2010/07/24 00:45:54 jym Exp $");

#include <machine/bioscall.h>

#include "assym.h"

/* LINTSTUB: include <sys/types.h> */
/* LINTSTUB: include <machine/bioscall.h> */

	.section ".rodata"
_C_LABEL(biostramp_image):
	.globl	_C_LABEL(biostramp_image)

8:
#include "i386/bioscall/biostramp.inc"
9:

	.globl	_C_LABEL(biostramp_image_size)
_C_LABEL(biostramp_image_size):
	.long	9b - 8b

	.text
/*
 * void bioscall(int function, struct bioscallregs *regs):
 * 	call the BIOS interrupt "function" from real mode with
 *	registers as specified in "regs"
 *	for the flags, though, only these flags are passed to the BIOS--
 *	the remainder come from the flags register at the time of the call:
 *	(PSL_C|PSL_PF|PSL_AF|PSL_Z|PSL_N|PSL_D|PSL_V)
 *
 *	Fills in *regs with registers as returned by BIOS.
 */
/* LINTSTUB: Func: void bioscall(int function, struct bioscallregs *regs) */
NENTRY(bioscall)
	pushl	%ebp
	movl	%esp,%ebp		/* set up frame ptr */

	/* install lwp0 pmap */
	movl	_C_LABEL(kernel_pmap_ptr),%eax
	pushl	%eax
	call	_C_LABEL(cpu_load_pmap)
	addl	$4,%esp

	movl	$(BIOSTRAMP_BASE),%eax	/* address of trampoline area */
	pushl	12(%ebp)
	pushl	8(%ebp)
	call	*%eax			/* machdep.c initializes it */
	addl	$8,%esp			/* clear args from stack */

	/* restore pmap - saved value is in curcpu()->ci_pmap */
	movl	%fs:(CPU_INFO_PMAP),%eax
	pushl	%eax
	call	_C_LABEL(cpu_load_pmap)
	addl	$4,%esp

	leave
	ret
