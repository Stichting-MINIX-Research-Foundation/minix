/*	$NetBSD: bzero.S,v 1.4 2008/04/28 20:22:57 martin Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ross Harvey.
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
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,  PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,  OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include	<machine/asm.h>

	.file	"bzero.c"
ENTRY(bzero)
	mr	5, 4		# translate to memcpy signature
	li	4, 0		# add missing memcpy param
ENTRY(memset)
	cmpldi	7, 5, 7		# only optimize if len >= 8
	mr	9, 3
	ble	7, 2f
	rldicl.	0, 3, 0, 61	# only optimize if aligned
	beq	0, 4f
2:
	cmpdi	7, 5, 0	
	beqlr	7
3:
	mtctr	5
	rldicl	0, 4, 0, 56
	.p2align 4,, 15
2:
	stb	0, 0(9)		# byte-at-a-time loop
	addi	9, 9, 1
	bdnz	2b
	blr			# done
4:
	rldicl	4, 4, 0, 56	# construct parallel store
	cmpldi	7, 5, 31
	mr	10, 3
	sldi	0, 4, 8
	or	0, 0, 4
	sldi	9, 0, 16
	or	0, 0, 9
	sldi	11, 0, 32
	or	0, 0, 11
	ble	7, 5f
	addi	9, 5, -32
	srdi	9, 9, 5
	addi	9, 9, 1
	mtctr	9
	.p2align 4,, 15
2:
	std	0, 0(10)	# 8-way + unrolled store loop
	std	0, 8(10)
	addi	5, 5, -32	# schedule count decrement
	std	0, 16(10)
	std	0, 24(10)
	addi	10, 10, 32
	bdnz	2b
	cmpldi	7, 5, 7
	ble	7, 8f
5:
	addi	9, 5, -8
	srdi	9, 9, 3
	addi	9, 9, 1
	mtctr	9
	.p2align 4,, 15
14:
	std	0, 0(10)	# 8-way not-unrolled loop
	addi	5, 5, -8
	addi	10, 10, 8
	bdnz	14b
8:
	cmpdi	7, 5, 0
	mr	9, 10
	bne	7, 3b
	blr
	.long	0
	.byte	0, 0, 0, 0, 0, 0, 0, 0
	.size	.memset, .-.memset
