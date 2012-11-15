/*	$NetBSD: ffs.S,v 1.4 2011/07/04 11:35:26 mrg Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#include <mips/asm.h>

RCSID("$NetBSD: ffs.S,v 1.4 2011/07/04 11:35:26 mrg Exp $")

/* bit = ffs(value) */

	.text
	.set	noreorder

WEAK_ALIAS(__ffssi2,ffs)
#if __mips == 64 || __mips == 32
LEAF(ffs)
#ifndef _LP64
XLEAF(ffsl)
#endif
	.set	push
	.set	mips32
	li	v1, 32
#if __mips == 64
	sll	a0, a0, 0
#endif
	negu	a1, a0
	and	a0, a1
	clz	v0, a0
	j	ra
	 subu	v0, v1, v0
	.set	pop
END(ffs)
#if defined(_LP64) && __mips == 64
LEAF(ffsl)
	li	v1, 64
	negu	a1, a0
	and	a0, a1
	dclz	v0, a0
	j	ra
	 subu	v0, v1, v0
END(ffsl)
#endif
#else /* __mips != 64 && __mips != 32 */

#ifdef _LP64
XLEAF(ffsl)
	beqz	a0, 6f			# fast escape if 0
	 li	v0, 0

	li	v0, 1
	li	a3, 0xffffffff		# initial mask
	b	1f
	 li	a2, 32			# bit count of mask
#endif /* _LP64 */
LEAF(ffs)
#ifndef _LP64
XLEAF(ffsl)
#endif /* !_LP64 */
	beqz	a0, 6f
	 li	v0, 0

	li	v0, 1
	li	a3, 0xffff		# initial mask
	li	a2, 16			# bit count of mask
1:
	and	v1, a0, a3		# focus no lower half of bits left
	bnez	v1, 2f			# any of the lower half set?
	 nop
	addu	v0, a2			# nope, then bit is in the upper half
#ifdef _LP64
	dsrlv	a0, a0, a2		# discard low bits
#else
	srlv	a0, a0, a2		# discard low bits
#endif
2:
	srl	a2, 1			# divide bit count by 2
	bnez	a2, 1b			# still bits left to text?
	 srlv	a3, a3, a2		# shrink mask in half
6:
	j	ra
	 nop
END(ffs)
#endif /* __mips == 64 || __mips == 32 */
