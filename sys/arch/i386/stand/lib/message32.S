/*	$NetBSD: message32.S,v 1.1 2009/11/19 22:13:17 dsl Exp $	*/

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
 * Output messages directly to serial port from 32bit mode.
 * Useful for debugging when the real-prot is suspect.
 *
 * %ds:dump_eax_buff must be somewhere it is safe to write 12 bytes.
 */

#ifndef COM_PORT_VAL
#define COM_PORT_VAL $0x3f8	/* Standard address COM1 (dty0) */
#endif

	.globl	message32
	.code32
message32:
	pusha   
message32_1:
	lodsb   
2:
	mov     COM_PORT_VAL, %dx
	outb    %al, %dx
	add     $5, %dl
3:      inb     %dx
	test    $0x40, %al
	jz      3b

	lodsb
	test    %al, %al
	jnz     2b
	popa
	ret     

	.globl	dump_eax32
dump_eax32:
	pusha
	movl	$dump_eax_buff, %esi
	mov	%esi, %edi
	push	$8
	pop	%ecx
1:	roll	$4, %eax
	push	%eax
	andb	$0x0f, %al
	addb	$0x30, %al			/* 30..3f - clear AF */
#if 1 /* 5 bytes to generate real hex... */
	daa					/* 30..39, 40..45 */
	addb	$0xc0, %al			/* f0..f9, 00..05 */
	adcb	$0x40, %al			/* 30..39, 41..46 */
#endif
	movb	%al,(%edi)
	inc	%edi
	pop	%eax
	loop	1b
	push	$0x20				/* space + 3 NULs */
	pop	(%edi)
	jmp	message32_1
