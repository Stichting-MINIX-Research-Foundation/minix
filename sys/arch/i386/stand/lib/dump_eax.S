/*	$NetBSD: dump_eax.S,v 1.4 2009/11/19 22:08:14 dsl Exp $	*/

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

/* This is useful for debugging - although you may need to
 * delete some code to fit it in.
 * %ds:dump_eax_buff must be somewhere it is safe to write 10 bytes.
 */

ENTRY(dump_eax)
	.code16
	pusha					/* saves bottom 16 bits only! */
	movw	$dump_eax_buff, %si
	mov	%si, %di
	movw	$8, %cx
1:	roll	$4, %eax
	push	%ax
	andb	$0x0f, %al
	addb	$0x30, %al			/* 30..3f - clear AF */
#if 1 /* 5 bytes to generate real hex... */
	daa					/* 30..39, 40..45 */
	addb	$0xc0, %al			/* f0..f9, 00..05 */
	adcb	$0x40, %al			/* 30..39, 41..46 */
#endif
	mov	%al,(%di)
	inc	%di
	pop	%ax
	loop	1b
	movw	$0x20,(%di)			/* space + NIL */
	jmp	message_1
