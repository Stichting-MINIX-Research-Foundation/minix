/*	$NetBSD: rtld_start.S,v 1.17 2008/04/28 20:23:04 martin Exp $	*/

/*-
 * Copyright (c) 2000 Eduardo Horvath.
 * Copyright (c) 1999, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas, Paul Kranenburg and by Charles M. Hannum.
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
#define	_LOCORE
#include <machine/frame.h>

/*
 * ELF:
 *	On startup the stack should contain 16 extended word register save area,
 *	followed by the arg count, etc.
 *
 * _rtld() expects the stack pointer to point to two longwords for argument
 *	return followed by argc, etc.  We need to create a pointer to
 *	&argc + 16 and pass that in.  The return args will be in those locations.
 * 
 * NB:	We are violating the ELF spec by passing a pointer to the ps strings in
 * 	%g1 instead of a termination routine.
 */

	.register	%g2,#scratch
	.register	%g3,#scratch

/* Offset of ARGC from bottom of stack */
#define	ARGC	176
	
	.section	".text"
	.align	4
	.global	_rtld_start
	.type	_rtld_start,@function
_rtld_start:
	mov	0, %fp			/* Erect a fence post for ourselves */
	mov	%g1, %l1		/* save ps_strings */
	sub	%sp, 48+16, %sp		/* Make room for return args */

	sethi	%hi(_GLOBAL_OFFSET_TABLE_-4), %l7
	call	0f
	 add	%l7, %lo(_GLOBAL_OFFSET_TABLE_+4), %l7
	call	_DYNAMIC+8
0:	add	%l7, %o7, %l7		/* real &_GLOBAL_OFFSET_TABLE_ */
	ld	[%o7+8], %o0		/* load stub call instruction */
	ldx	[%l7], %l0		/* base-relative &_DYNAMIC */
	sll	%o0, 2, %o0		/* extract PC offset */
	sra	%o0, 0, %o0		/* sign-extend */

	add	%o0, %o7, %o0		/* real &_DYNAMIC */
	sub	%o0, %l0, %l0		/* relocbase */
	call	_rtld_relocate_nonplt_self
	 mov	%l0, %o1		/* relocbase */

	mov	%l0, %o1		/* relocbase */
	call	_rtld
	 add	%sp, BIAS + ARGC, %o0	/* &argc - 16 */

	ldx	[%sp + BIAS + ARGC], %g3	/* arg: cleanup */
	ldx	[%sp + BIAS + ARGC + 8], %g2	/* arg: obj */
	add	%sp, 48+16, %sp		/* restore stack pointer */

	jmp	%o0
	 mov	%l1, %g1		/* restore ps_strings */


	/*
	 * We have two separate entry points to the runtime linker.
	 * I'm implementing this following the SPARC v9 ABI spec.
	 *
	 * _rtld_bind_start_0(y, x) is called from .PLT0, and is used for
	 * PLT entries above 32768.
	 *
	 * _rtld_bind_start_1(y, x) is called from .PLT1, and is used for
	 * PLT entries below 32768.
	 *
	 * The first two entries of PLT2 contain the xword object pointer.
	 *
	 * These routines are called with two longword arguments, 
	 * x and y.  To calculate the address of the entry,
	 * _rtld_bind_start_1(y, x) does:
	 *
	 *	n = x >> 15;
	 *
	 * and _rtld_bind_start_0(y, x) does:
	 *
	 *	i = x - y + 8 - 32768*32;
	 *	n = 32768 + (i/5120)*160 + (i%5120)/24;
	 *
	 * Neither routine needs to issue a save since it's already been
	 * done in the PLT entry.
	 */

	.section	".text"
	.align	4
	.global	_rtld_bind_start_0
	.type	_rtld_bind_start_0,@function
_rtld_bind_start_0:	# (y, x)
	/* %o0 = obj->pltgot[6] */
	/* %o1 = plt[4] */
	/* %o1 - %o0 + 8 == offset of plt[] from obj->pltgot[] */
	/* -32768*32 to get offset from beginning of upper PLT section */

	sethi	%hi(32768*32-8), %l1
	sub	%o1, %o0, %l0		/* i = x - y */
	or	%l1, %lo(32768*32-8), %l1
	sub	%l0, %l1, %l0		/* i = x - y + 8 - 32768*32 */

	ldx	[%o0 + (10*4)], %o0	/* Load object pointer from PLT2 */

	sethi	%hi(5120), %l1
	sdivx	%l0, %l1, %l1		/* Calculate i/5120 */
	sllx	%l1, 2, %l2
	add	%l2, %l1, %l2
	sllx	%l2, 10, %l2
	sub	%l0, %l2, %l2		/* And i%5120 */
	
	/* Let the division churn for a bit. */
	sdivx	%l2, 24, %l4		/* (i%5120)/24 */

	/* 160 is (32 * 5) or (32 * (4 + 1)) */
	sllx	%l1, 2, %l3		/* 4 * (i/5120) */
	add	%l1, %l3, %l3		/* 5 * (i/5120) */
	sllx	%l3, 5, %l3		/* 32 * 5 * (i/5120) */

	sethi	%hi(32768), %l6
	add	%l3, %l4, %l5		/* %l5 = (i/5120)*160 + (i%5120)/24; */
	
	call	_rtld_bind		/* Call _rtld_bind(obj, offset) */
	 add	%l5, %l6, %o1		/* %o1 = 32768 + ... */

	jmp	%o0			/* return value == function address */
	 restore			/* Dump our stack frame */
	
	.section	".text"
	.align	4
	.global	_rtld_bind_start_1
	.type	_rtld_bind_start_1,@function
_rtld_bind_start_1:	# (y, x)
	ldx	[%o0 + (2*4)], %o0	/* Load object pointer from PLT2 */
	
	call	_rtld_bind		/* Call _rtld_bind(obj, offset) */
	 srax	%o1, 15, %o1		/* %o1 is the index to our PLT slot */

	jmp	%o0			/* return value == function address */
	 restore			/* Dump our stack frame */

