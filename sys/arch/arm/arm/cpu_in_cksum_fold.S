/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

/*
 * This file is intended to be included at the end of a in_cksum routine
 * to reduce the 33-bit sum in <carry>, ip to a 16-bit return value.
 */
	/*
	 * We now have a 33-bit (r0 + carry) sum which needs to resolved to a
	 * 16-bit sum.  But first, let's put 0xffff in a register.
	 */
#ifdef _ARM_ARCH_7
	movw	r1, #0xffff		/* load 0xffff */
#else
	mov	r1, #0x10000		/* load 0x10000 */
	sub	r1, r1, #1		/* subtract by 1 to get 0xffff */
#endif

	/*
	 * Add the final carry bit.  If it overflows, we have a 33-bit value
	 * of 0x1.0000.0000 which we know is just equivalent to 1.  Since we
	 * return a complement of the lower halfword, that's 0xfffe.
	 */
	adcs	ip, ip, #0		/* add final carry bit */
	beq	1f			/*   0?  return 0xfffe */

	/*
	 * Now prevent the adding of 0xffff to 0xffff by making sure the upper
	 * halfword isn't 0xffff.  If it is, just complement all 32-bits
	 * which clears the upper halfword and complements the lower halfword.
	 */
	cmp	ip, r1, lsl #16		/* is the upper halfword 0xffff? */
	beq	2f			/*   yes, complement and return */
	/*
	 * Finally add the lower halfword to the upper halfword.  If we have
	 * a result >= 0x10000, carry will be set.  The maximum result will
	 * be 0x[1]fffe.  So if the carry bit is set, just add 0x10000
	 * (which is equivalent to subtracting 0xffff.0000).
	 */
	adds	ip, ip, ip, lsl #16
	addcs	ip, ip, #0x10000
	eor	r0, r1, ip, lsr #16
	RET

1:	sub	r0, r1, #1		/* set return value to 0xfffe */
	RET				/* return */
2:	mvn	r0, ip			/* complement */
	RET				/* return */

