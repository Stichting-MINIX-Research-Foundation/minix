/*	$NetBSD: setjmp.S,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

/*
 * Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.
 * This program and the accompanying materials are licensed and made
 * available under the terms and conditions of the BSD License which
 * accompanies this distribution.  The full text of the license may
 * be found at http://opensource.org/licenses/bsd-license.php.
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" 
 * BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED.
 */
	.text
	.arm
	.globl	setjmp
	.type	setjmp, %function
setjmp:
	mov	r3, r13
	stmia	r0, {r3-r12,r14}
	eor	r0, r0, r0
	bx	lr

	.globl	longjmp
	.type	longjmp, %function
longjmp:
	ldmia	r0, {r3-r12,r14}
