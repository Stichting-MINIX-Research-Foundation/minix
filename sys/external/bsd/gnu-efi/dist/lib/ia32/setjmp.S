/*	$NetBSD: setjmp.S,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

/*
 * Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.
 * This program and the accompanying materials are licensed and made 
available
 * under the terms and conditions of the BSD License which accompanies 
this
 * distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php.
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" 
BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED.
 */
	.text
	.globl	setjmp
#ifndef __MINGW32__
	.type setjmp, @function
#else
	.def setjmp; .scl 2; .type 32; .endef
#endif
setjmp:
	pop	%ecx
	movl	(%esp), %edx
	movl	%ebx, (%edx)
	movl	%esi, 4(%edx)
	movl	%edi, 8(%edx)
	movl	%ebp, 12(%edx)
	movl	%esp, 16(%edx)
	xorl	%eax, %eax
	jmp	*%ecx

	.globl	longjmp
#ifndef __MINGW32__
	.type	longjmp, @function
#else
	.def longjmp; .scl 2; .type 32; .endef
#endif
longjmp:
	pop	%eax
	pop	%edx
	pop	%eax
	movl	(%edx), %ebx
	movl	4(%edx), %esi
	movl	8(%edx), %edi
