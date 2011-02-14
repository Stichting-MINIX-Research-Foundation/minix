/*	$NetBSD: strlen.S,v 1.6 2011/01/15 07:31:12 matt Exp $ */

/*-
 * Copyright (C) 2001	Martin J. Laubach <mjl@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*----------------------------------------------------------------------*/

#include <machine/asm.h>

__RCSID("$NetBSD: strlen.S,v 1.6 2011/01/15 07:31:12 matt Exp $");

/*----------------------------------------------------------------------*/
/* The algorithm here uses the following techniques:

   1) Given a word 'x', we can test to see if it contains any 0 bytes
      by subtracting 0x01010101, and seeing if any of the high bits of each
      byte changed from 0 to 1. This works because the least significant
      0 byte must have had no incoming carry (otherwise it's not the least
      significant), so it is 0x00 - 0x01 == 0xff. For all other
      byte values, either they have the high bit set initially, or when
      1 is subtracted you get a value in the range 0x00-0x7f, none of which
      have their high bit set. The expression here is
      (x + 0xfefefeff) & ~(x | 0x7f7f7f7f), which gives 0x00000000 when
      there were no 0x00 bytes in the word.

   2) Given a word 'x', we can test to see _which_ byte was zero by
      calculating ~(((x & 0x7f7f7f7f) + 0x7f7f7f7f) | x | 0x7f7f7f7f).
      This produces 0x80 in each byte that was zero, and 0x00 in all
      the other bytes. The '| 0x7f7f7f7f' clears the low 7 bits in each
      byte, and the '| x' part ensures that bytes with the high bit set
      produce 0x00. The addition will carry into the high bit of each byte
      iff that byte had one of its low 7 bits set. We can then just see
      which was the most significant bit set and divide by 8 to find how
      many to add to the index.
      This is from the book 'The PowerPC Compiler Writer's Guide',
      by Steve Hoxey, Faraydon Karim, Bill Hay and Hank Warren.
*/
/*----------------------------------------------------------------------*/

		.text
		.align 4

ENTRY(strlen)

		/* Setup constants */
		lis	%r10, 0x7f7f
		lis	%r9, 0xfefe
		ori	%r10, %r10, 0x7f7f
		ori	%r9, %r9, 0xfeff

		/* Mask out leading bytes on non aligned strings */
		rlwinm.	%r8, %r3, 3, 27, 28	/* leading bits to mask */
#ifdef _LP64
		clrrdi	%r5, %r3, 2		/*  clear low 2 addr bits */
#else
		clrrwi	%r5, %r3, 2		/*  clear low 2 addr bits */
#endif
		li	%r0, -1
		beq+	3f			/* skip alignment if already */
						/* aligned */

		srw	%r0, %r0, %r8		/* make 0000...1111 mask */

		lwz	%r7, 0(%r5)
		nor	%r0, %r0, %r0		/* invert mask */
		or	%r7, %r7, %r0		/* make leading bytes != 0 */
		b	2f

3:		subi	%r5, %r5, 4

1:		lwzu	%r7, 4(%r5)		/* fetch data word */

2:		nor	%r0, %r7, %r10		/* do step 1 */
		add	%r6, %r7, %r9
		and.	%r0, %r0, %r6

		beq+	1b			/* no NUL bytes here */
	
		and	%r8, %r7, %r10		/* ok, a NUL is somewhere */
		or	%r7, %r7, %r10		/* do step 2 to find out */
		add	%r0, %r8, %r10		/* where */
		nor	%r8, %r7, %r0

		cntlzw	%r0, %r8		/* offset from this word */
		srwi	%r4, %r0, 3

		add	%r4, %r5, %r4		/* r4 contains end pointer */
		/* NOTE: Keep it so this function returns the end pointer
		   in r4, so we can it use from other str* calls (strcat
		   comes to mind */

		subf	%r3, %r3, %r4
		blr
END(strlen)
/*----------------------------------------------------------------------*/
