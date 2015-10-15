/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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
#include <sys/errno.h>

RCSID("$NetBSD: n_scalbn.S,v 1.5 2014/02/03 21:22:21 martin Exp $")

WEAK_ALIAS(scalbn,_scalbn)
WEAK_ALIAS(scalbnl,_scalbnl)
WEAK_ALIAS(scalbnf,_scalbnf)
WEAK_ALIAS(ldexp,_scalbn)
WEAK_ALIAS(ldexpf,_scalbnf)
WEAK_ALIAS(ldexpl,_scalbnl)

ENTRY(_scalbnf, 0)
	cvtfd	4(%ap), %r0
	movl	8(%ap), %r2
	jr	1f

ALTENTRY(_scalbnl)
ENTRY(_scalbn, 0)
	movd	4(%ap), %r0
	movl	12(%ap), %r2

	/*
	 * First see if the result will be within bounds
	 */
1:	cmpl	%r2,$(256+55)
	jgeq	overflow
	cmpl	%r2,$-(256+55)
	jleq	underflow

	extzv	$7, $9, %r0, %r3	/* extract exponent */
	jneq	5f			/* 0 * N is still 0 */
	cmpw	%r3, $-256		/* is this a NAN? */
	jgtr	2f			/*    nope */
	ret

2:	movzbl	%r3, %r3		/* clear sign bit */
	addl2	%r2, %r3		/* add to exponent */
	jleq	underflow		/*   <= 0? */

3:	jbs	$8, %r3, overflow	/* did exponent overflow? */
	insv	$7, $8, %r3, %r0	/* replace exponent */
	ret

underflow:
	clrq	%r0			/* return 0.0 */
	jr	copysign		/* or mabye -0.0 */
overflow:
	pushl	$ERANGE			/* overflow */
	calls	$1, _C_LABEL(infnan)	/* possibly signal */
copysign:
	jbc	$15, 4(%ap), 5f		/* if clear, don't set sign-bit */
	bisw2	$0x8000, %r0		/* preserve sign-bit */
5:
	ret
