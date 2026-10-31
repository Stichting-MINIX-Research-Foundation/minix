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
	.p2align 3

#define GPR_LAYOUT			\
	REG_PAIR (x19, x20, 0);		\
	REG_PAIR (x21, x22, 16);	\
	REG_PAIR (x23, x24, 32);	\
	REG_PAIR (x25, x26, 48);	\
	REG_PAIR (x27, x28, 64);	\
	REG_PAIR (x29, x30, 80);	\
	REG_ONE (x16, 96)

#define FPR_LAYOUT			\
	REG_PAIR(d8, d9, 112);		\
	REG_PAIR(d10, d11, 128);	\
	REG_PAIR(d12, d13, 144);	\
	REG_PAIR(d14, d15, 160);

#define REG_PAIR(REG1, REG2, OFFS)      stp REG1, REG2, [x0, OFFS]
#define REG_ONE(REG1, OFFS)             str REG1, [x0, OFFS]

	.globl	setjmp
	.type	setjmp, @function
setjmp:
	mov	x16, sp
	GPR_LAYOUT
	FPR_LAYOUT
	mov	w0, #0
	ret

#undef REG_PAIR
#undef REG_ONE

#define REG_PAIR(REG1, REG2, OFFS)      ldp REG1, REG2, [x0, OFFS]
#define REG_ONE(REG1, OFFS)             ldr REG1, [x0, OFFS]

	.globl	longjmp
	.type	longjmp, @function
longjmp:
	GPR_LAYOUT
	FPR_LAYOUT
	mov	sp, x16
	cmp	w1, #0
	mov	w0, #1
	csel	w0, w1, w0, ne
	br	x30
