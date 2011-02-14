/*	$NetBSD: bzero.S,v 1.1 2005/12/20 19:28:50 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *
 * from: Header: bzero.s,v 1.1 92/06/25 12:52:46 torek Exp
 */

#include <machine/asm.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	.asciz "@(#)bzero.s	8.1 (Berkeley) 6/4/93"
#else
	RCSID("$NetBSD: bzero.S,v 1.1 2005/12/20 19:28:50 christos Exp $")
#endif
#endif  /* LIBC_SCCS and not lint */

/*
 * We should unroll the loop, but at the moment this would
 * gain nothing since the `std' instructions are what limits us.
 */

#ifdef MEMSET
/*
 * void *
 * memset(void *addr, int pattern, size_t len)
 */
ENTRY(memset)
	! %o0 = addr, %o1 = pattern, %o2 = len
	/*
	 * Expand the byte pattern to fill 64 bits in an even-aligned
	 * register pair; shuffle arguments to match those of bzero.
	 */
	and	%o1, 0xff, %o3
	mov	%o2, %o1		! shuffle argument
	sll	%o3, 8, %o2
	or	%o2, %o3, %o2
	mov	%o0, %g1		! save original pointer
	sll	%o2, 16, %o3
	or	%o2, %o3, %o2
	! Optimize a common case: addr and len are both multiples of 8.
	or	%o0, %o1, %o5
	btst	7, %o5			! ((addr | len) & 7) != 0?
	bnz	1f			! if so, cannot optimize
	 mov	%o2, %o3		! in any case, complete pat expansion
#else
/*
 * void
 * bzero(void *addr, size_t len)
 */
ENTRY(bzero)
	! %o0 = addr, %o1 = len

	clr	%o2
	! Optimize a common case: addr and len are both multiples of 8.
	or	%o0, %o1, %o5
	btst	7, %o5			! ((addr | len) & 7) != 0?
	bnz	1f			! if so, cannot optimize
	 clr	%o3			! in any case, we want o3=0
#endif

	/* `Good' operands, can just store doubles. */
0:
	deccc	8, %o1			! while ((len -= 8) >= 0)
	bge,a	0b
	 std	%o2, [%o0 + %o1]	!	*(quad *)(addr + len) = 0;
	retl
	 nop

	/*
	 * Either the address is unaligned, or the count is not a
	 * multiple of 8, or both.  We will have to align the address
	 * in order to use anything `better' than stb.
	 */
1:
	cmp	%o1, 15			! len >= 15?
	bge,a	Lstd			! yes, use std
	 btst	1, %o0			! (but first check alignment)

	! not enough to bother: do byte-at-a-time loop.
2:
	deccc	%o1			! while (--len >= 0)
	bge,a	2b
	 stb	%o2, [%o0 + %o1]	!	addr[len] = 0;
	retl
	 nop

Lstd:
	/*
	 * There are at least 15 bytes to zero.
	 * We may have to zero some initial stuff to align
	 * the address.
	 */
	bz,a	1f			! if (addr & 1) {
	 btst	2, %o0
	stb	%o2, [%o0]		!	*addr = 0;
	inc	%o0			!	addr++;
	dec	%o1			!	len--;
	btst	2, %o0			! }
1:
	bz,a	1f			! if (addr & 2) {
	 btst	4, %o0
	sth	%o2, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
	dec	2, %o1			!	len -= 2;
	btst	4, %o0			! }
1:
	bz	1f			! if (addr & 4) {
	 dec	8, %o1
	st	%o2, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
	dec	4, %o1			!	len -= 4;
					! }
	/*
	 * Address is double word aligned; len is 8 less than
	 * the number of bytes remaining (i.e., len is 0 if
	 * the remaining count is 8, 1 if it is 9, etc.).
	 */
1:
	std	%o2, [%o0]		! do {
2:					!	*(quad *)addr = 0;
	inc	8, %o0			!	addr += 8;
	deccc	8, %o1			! } while ((len -= 8) >= 0);
	 bge,a	2b
	std	%o2, [%o0]

	/*
	 * Len is in [-8..-1] where -8 => done, -7 => 1 byte to zero,
	 * -6 => two bytes, etc.  Mop up this remainder, if any.
	 */
	btst	4, %o1
	bz	1f			! if (len & 4) {
	 btst	2, %o1
	st	%o2, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
1:
	bz	1f			! if (len & 2) {
	 btst	1, %o1
	sth	%o2, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
1:
	bnz,a	1f			! if (len & 1)
	 stb	%o2, [%o0]		!	*addr = 0;
1:
	retl
#ifdef MEMSET
	 mov	%g1, %o0		! restore original pointer
#else
	 nop
#endif
