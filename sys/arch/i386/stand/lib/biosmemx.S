/*	$NetBSD: biosmemx.S,v 1.9 2008/10/14 14:18:11 ad Exp $	*/

/*
 * Copyright (c) 1997, 1999
 *	Matthias Drochner.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <machine/asm.h>

	.text

/* int getextmem2(int buffer[2])
   return: 0=OK, -1=error
   buffer[0]: extmem kBytes below 16M (max 15M/1024)
   buffer[1]: extmem above 16M, in 64k units
*/
ENTRY(getextmem2)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	call	_C_LABEL(prot_to_real)
	.code16

	xorl	%ebx, %ebx
	movl	$0xe801, %eax
	int	$0x15
	pushf

	movw	%si, %ax
	orw	%si, %bx
	jz	1f		/* if zero use configured values */
	movw	%cx, %ax	/* k below 16M (max 0x3c00 = 15MB) */
	movw	%dx, %bx	/* 64k above 16M */
1:
	popf
	setc	%bl

	calll	_C_LABEL(real_to_prot)
	.code32

	movl	8(%ebp), %edi
	xorl	%eax, %eax
	movw	%cx, %ax
	stosl
	movw	%dx, %ax
	stosl
	movb	%bl, %al
	cbw

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/* int getmementry(int *iterator, buffer[5])
   return: 0=ok, else error
   buffer[0]: start of memory chunk
   buffer[2]: length (bytes)
   buffer[4]: type
*/
ENTRY(getmementry)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	movl	8(%ebp), %eax
	movl	0(%eax), %ebx		/* index */
	movl	$20, %ecx		/* Buffer size */
	movl	$0x534d4150, %edx	/* "SMAP" */
	movl	12(%ebp), %edi		/* buffer address */

	call	_C_LABEL(prot_to_real)
	.code16

	push	%di
	shrl	$4, %edi
	mov	%ds, %ax
	add	%di, %ax
	mov	%ax, %es
	pop	%di
	and	$0xf, %di		/* buffer addres now in ES:DI */

	movl	$0xe820, %eax		/* Some BIOS check EAX value */
	int	$0x15

	setc	%cl

	calll	_C_LABEL(real_to_prot)
	.code32

	movl	8(%ebp), %eax
	movl	%ebx, 0(%eax)		/* updated index */
	xorl	%eax, %eax
	movb	%cl, %al

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/* int biosA20(void)
   return: 0=ok, else error
*/
ENTRY(biosA20)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	call	_C_LABEL(prot_to_real)
	.code16

	movl	$0x2401, %eax
	int	$0x15
	setc	%cl

	calll	_C_LABEL(real_to_prot)
	.code32

	movzbl	%cl, %eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret
