/* $NetBSD: memcpy.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

RCSID("$NetBSD: memcpy.S,v 1.1 2014/08/10 05:47:35 matt Exp $")

/* LINTSTUB: void *memcpy(void * restrict, const void * restrict, size_t); */

ENTRY(memcpy)
	mov	x10, x0
	mov	x11, x1
	cbz	x2, .Lmemcpy_ret

	cmp	x2, #7
	b.ls	.Lmemcpy_last_dword

	ands	x3, x10, #7
	b.eq	.Lmemcpy_dword_aligned

/*
 * The dst address doesn't have dword alignment.  The src address may or may
 * not have the same alignment.  Make dst dword aligned.  Hope src will be
 * dword aligned but if it isn't, take advantage of unaligned access.
 */
	add	x2, x2, x3		/* add unalignment to length */
	sub	x2, x2, #8		/* now subtract a dword */

	tbz	x10, #0, .Lmemcpy_hword_aligned
	ldrb	w4, [x11], #1
	strb	w4, [x10], #1
.Lmemcpy_hword_aligned:
	tbz	x10, #1, .Lmemcpy_word_aligned
	ldrh	w4, [x11], #2
	strh	w4, [x10], #2
.Lmemcpy_word_aligned:
	tbz	x10, #2, .Lmemcpy_dword_aligned
	ldr	w4, [x11], #4
	str	w4, [x10], #4
.Lmemcpy_dword_aligned:
	/*
	 * destination is now dword aligned.
	 */
	subs	x2, x2, #32
	b.mi	.Lmemcpy_last_oword

.Lmemcpy_oword_loop:
	ldp	x4, x5, [x11], #16
	ldp	x6, x7, [x11], #16
	stp	x4, x5, [x10], #16
	stp	x6, x7, [x10], #16
	cbz	x2, .Lmemcpy_ret
	subs	x2, x2, #32
	b.pl	.Lmemcpy_oword_loop

.Lmemcpy_last_oword:
	/*
	 * We have 31 bytes or less to copy.  First see if we can write a qword
	 */
	tbz	x2, #4, .Lmemcpy_last_qword
	ldp	x4, x5, [x11], #16		/* read word */
	stp	x4, x5, [x10], #16		/* write word */

.Lmemcpy_last_qword:
	/*
	 * We have 15 bytes or less to copy.  First see if we can write a dword
	 */
	tbz	x2, #3, .Lmemcpy_last_dword
	ldr	x4, [x11], #8		/* read word */
	str	x4, [x10], #8		/* write word */

.Lmemcpy_last_dword:
	/*
	 * We have 7 bytes or less to copy.  First see if we can write a word
	 */
	tbz	x2, #2, .Lmemcpy_last_word
	ldr	w4, [x11], #4		/* read word */
	str	w4, [x10], #4		/* write word */

.Lmemcpy_last_word:
	/*
	 * We have 3 bytes or less to copy.  First see if we can write a hword
	 */
	tbz	x2, #1, .Lmemcpy_last_hword
	ldrh	w4, [x11], #2
	strh	w4, [x10], #2

.Lmemcpy_last_hword:
	/*
	 * We have 1 or none bytes to copy.
	 */
	tbz	x2, #0, .Lmemcpy_ret
	ldrb	w4, [x11]
	strb	w4, [x10]

.Lmemcpy_ret:
	ret
END(memcpy)
