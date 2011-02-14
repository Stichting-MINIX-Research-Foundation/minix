/*	$NetBSD: byte_swap_8.S,v 1.2 2009/12/14 00:39:00 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <mips/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: byte_swap_8.S,v 1.2 2009/12/14 00:39:00 matt Exp $")
#endif /* LIBC_SCCS and not lint */

#undef _LOCORE
#define _LOCORE		/* XXX not really, just assembly-code source */
#include <machine/endian.h>

NLEAF(bswap64)	# a0 = 0xffeeddccbbaa9988 return 0x8899aabbccddeeff
#if (__mips == 32 || __mips == 64) && __mips_isa_rev == 2
#if !defined(__mips_o32)
	/*
	 * If we are on MIPS32r2 or MIPS64r2 use the new instructions.
	 */
	dsbh	v0, a0			# dwords swap bytes within halfwords
	dshd	v0, v0			# dwords swap halwords within dwords
	j	ra
#else /* defined(__mips_o32) */
	/*
	 * If we are on MIPS32r2 or MIPS64r2 use the new instructions.
	 * (except we must use the 32bit versions) 
	 */
	wsbh	v1, a0			# word swap bytes within halfwords
	wsbh	v0, a1			# word swap bytes within halfwords
	rotr	v1, v1, 16		# rotate word 16bits and swap word
	rotr	v0, v0, 16		# rotate word 16bits and swap word
	j	ra
#endif /* defined(__mips_o32) */
#elif !defined(__mips_o32)
					# a0  = 0xffeeddccbbaa9988
	li	t0, 0xffff		# t0  = 0x000000000000ffff
	dsll	t1, t0, 32		# t1  = 0x0000ffff00000000
	or	t0, t1			# t0  = 0x0000ffff0000ffff
	dsll	t2, t0, 8		# t2  = 0x00ffff0000ffff00
	xor	t2, t0			# t2  = 0x00ff00ff00ff00ff
	/*
	 * We could swap by halfword, but that would be one instruction longer.
	 */
	dsrl	ta0, a0, 32		# ta0 = 0x00000000ffeeddcc
	dsll	ta1, a0, 32		# ta1 = 0xbbaa998800000000
	or	a1, ta0, ta1		# a1  = 0xbbaa9988ffeeddcc
					# words swapped
	and	ta0, a1, t0		# ta0 = 0x000099880000ddcc
	dsrl	ta1, a1, 16		# ta1 = 0x0000bbaa9988ffee
	and	ta1, t0			# ta1 = 0x0000bbaa0000ffee
	dsll	a2, ta0, 16		# a2  = 0x99880000ddcc0000
	or	a2, ta1			# a2  = 0x9988bbaaddccffee
					# halfwords swapped
	and	ta0, a2, t2		# ta0 = 0x008800aa00cc00ee
	dsrl	ta1, a2, 8		# ta1 = 0x009988bbaaddccff
	and	ta1, t2			# ta1 = 0x009900bb00dd00ff
	dsll	v0, ta0, 8		# v0  = 0x8800aa00cc00ee00
	or	v0, ta1			# v0  = 0x8899aabbccddeeff
					# bytes swapped
	j	ra
#else /* defined(__mips_o32) */
	/*
	 * 32bit ABI.
	 */
					# a0  = 0xccddeeff
					# a1  = 0x8899aabb
	srl	t0, a0, 24		# t0  = 0x000000cc
	srl	t1, a1, 24		# t1  = 0x00000088
	sll	ta0, a0, 24		# ta0 = 0xff000000
	sll	ta1, a1, 24		# ta1 = 0xbb000000
	or	ta0, ta0, t0		# ta0 = 0xff0000cc
	or	ta1, ta1, t1		# ta1 = 0xbb000088
	and	t0, a0, 0xff00		# t0  = 0x0000ee00
	and	t1, a1, 0xff00		# t1  = 0x0000aa00
	sll	t0, t0, 8		# t0  = 0x00ee0000
	sll	t1, t1, 8		# t1  = 0x00aa0000
	or	ta0, ta0, t0		# ta0 = 0xffee00cc
	or	ta1, ta1, t1		# ta1 = 0xbbaa0088
	srl	t0, a0, 8		# t0  = 0x00ccddee
	srl	t1, a1, 8		# t1  = 0x008899aa
	and	t0, t0, 0xff00		# t0  = 0x0000dd00
	and	t1, t1, 0xff00		# t1  = 0x00009900
	or	v1, ta0, t0		# v1  = 0xffeeddcc
	or	v0, ta1, t1		# v0  = 0xbbaa9988
	j	ra
#endif /* defined(__mips_o32) */
END(bswap64)
