/*	$NetBSD: biosmemps2.S,v 1.6 2011/06/16 13:27:59 joerg Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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

	.text

/* int getextmemp2(void buffer)
   call int 15 function 0xc7 - later PS/2s - RETURN MEMORY-MAP INFORMATION
   return: 0=OK, nonzero=error
   buffer: filled with memory-map table structure
*/
ENTRY(getextmemps2)
	.code32
	movl	%eax, %edx
	xorl	%eax, %eax
	pusha

	call	_C_LABEL(prot_to_real)
	.code16

	# do int15, function 0xc0 call to discover if C7h is supported
	movb	$0xc0, %ah
	int	$0x15
	setc	%cl
	jc	out		# 0xc0 not supported if carry set

	# check feature byte 2, bit 4 to see if return memory map is supported
	movb	%es:6(%bx), %al
	andb	$0x10, %al
	jnz	getmem		# 0xc7 supported
	
	# set %cl to indicate failure, and exit
	movb	$2, %cl
	jmp	out

getmem:
	# move the parameter to right register
	push	%ds
	movl	%edx, %esi
	andl	$0xf, %esi
	shrl	$4, %edx
	mov	%ds, %ax
	add	%dx, %ax
	mov	%ax, %ds

	# actually call int15, function 0xc7 now
	movb	$0xc7, %ah
	int	$0x15
	setc	%cl		# save carry
	pop	%ds

out:
	calll	_C_LABEL(real_to_prot)
	.code32

	movb	%cl, 28(%esp)
	popa
	ret
