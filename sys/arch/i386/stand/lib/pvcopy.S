/*	$NetBSD: pvcopy.S,v 1.2 2008/04/28 20:23:25 martin Exp $	*/

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

/*
 * Routines to copy to/from absolute virtual addresses.
 * Needed because the boot code runs with %ds having a 64k offset
 * whereas Unix runs with a zero offset.
 *
 * These routines are optimised for code space, not execution speed.
 */

/*
 * pbzero(void *dst, int cnt)
 *	zero absolute virtual memory
 */
ENTRY(pbzero)
	.code32
	push	%edi
	push	%es
	mov	12(%esp),%edi
	mov	16(%esp),%ecx

	mov	$flatdataseg, %ax	/* selector with offset == 0 */
	mov	%ax, %es
	xor	%eax,%eax

	cld
	rep
	stosb

	pop	%es
	pop	%edi
	ret

/*
 * vpbcopy(const void *src, void *dst, int cnt)
 *	Copy to absolute virtual address
 */
ENTRY(vpbcopy)
	.code32
	push	%esi
	push	%edi
	push	%es
	mov	16(%esp),%esi
	mov	20(%esp),%edi
	mov	24(%esp),%ecx

	mov	$flatdataseg, %ax	/* selector with offset == 0 */
	mov	%ax, %es
	xor	%eax,%eax

	cld
	rep
	movsb

	popl	%es
	popl	%edi
	popl	%esi
	ret

/*
 * pvbcopy(const void *src, void *dst, int cnt)
 *	Copy from absolute virtual address
 */
ENTRY(pvbcopy)
	.code32
	push	%esi
	push	%edi
	push	%ds
	mov	16(%esp),%esi
	mov	20(%esp),%edi
	mov	24(%esp),%ecx

	mov	$flatdataseg, %ax	/* selector with offset == 0 */
	mov	%ax, %ds
	xor	%eax,%eax

	cld
	rep
	movsb

	popl	%ds
	popl	%edi
	popl	%esi
	ret
