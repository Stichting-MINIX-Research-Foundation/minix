/*	$NetBSD: message.S,v 1.4 2009/11/19 22:10:03 dsl Exp $	*/

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

	.global	message, message_1

#if defined(BOOT_MSG_COM0) && !defined(COM_PORT_VAL)
#define COM_PORT_VAL $0x3f8		/* Value for COM1 */
#endif

/*
 * message: write the error message in %ds:%esi to the console
 */
message:
/*
 * BIOS call "INT 10H Function 0Eh" to write character to console
 *	Call with	%ah = 0x0e
 *			%al = character
 *			%bh = page
 *			%bl = foreground color
 */
	.code16
	pusha
message_1:			/* for dump_eax */
	lodsb
1:
#ifdef COM_PORT_VAL
	mov     COM_PORT_VAL, %dx
	outb    %al, %dx
	add     $5, %dl
2:	inb     %dx
	test    $0x40, %al
	jz      2b
#else
	movb	$0x0e, %ah
	movw	$0x0001, %bx
	int	$0x10
#endif
	lodsb
	testb	%al, %al
	jnz	1b

#ifdef MESSAGE_PAUSE
	/* Delay for about 1 second to allow message to be read */
	movb	$0x86, %ah
	mov	$16, %cx		/* about a second */
	int	$0x15			/* delay cx:dx usecs */
#endif
	popa
	ret
