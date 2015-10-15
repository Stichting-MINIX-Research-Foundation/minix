/*	$NetBSD: strlen.S,v 1.1 2014/09/03 19:34:25 matt Exp $ */

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

__RCSID("$NetBSD: strlen.S,v 1.1 2014/09/03 19:34:25 matt Exp $");

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

ENTRY(strlen)

		l.or	r12, r3, r0		/* save start */

		/* Setup constants */
		l.movhi	r13, 0x7f7f
		l.movhi	r15, 0xfefe
		l.ori	r13, r13, 0x7f7f
		l.ori	r15, r15, 0xfeff

1:		l.andi	r7, r12, 3		/* get low bits of start */
		l.sfeqi	r7, 0			/* all clear? */
		l.bf	3f			/*   yes, skip alignment */
		l.nop				/* -- delay slot -- */

		l.sub	r12, r12, r7		/* word align start */
		l.lwz	r8, 0(r12)		/* load data */
		l.addi	r6, r0, -1		/* r6 = 0xffffffff */
		l.slli	r5, r7, 3		/* bits to bytes */
		l.srl	r6, r6, r5		/* clear low (MSB) bytes */
		l.xori	r6, r6, -1		/* complement */
		l.or	r8, r8, r6		/* merge with loaded word */
		l.j	4f			/* and process */
		l.nop				/* -- delay-slot -- */

2:		l.addi	r12, r12, 4		/* advance to next word */
3:		l.lwz	r8, 0(r12)		/* fetch data word */

		// Step 1: (x + 0xfefefeff) & ~(x | 0x7f7f7f7f)
4:		l.or	r7, r8, r13		/* t0 = x | 0x7f7f7f7f */
		l.xori	r6, r7, -1		/* t1 = ~t0 */
		l.add	r5, r8, r15		/* t2 = x + 0xfefefeff */
		l.and	r4, r7, r5		/* t3 = t1 & t2 */
		l.sfeqi	r4, 0
		l.bf	2b			/* no NUL bytes here */
		l.nop				/* -- delay slot -- */
	
		// Step 2: ~(((x & 0x7f7f7f7f) + 0x7f7f7f7f) | x | 0x7f7f7f7f)
		l.and	r7, r8, r13		/* t0 = x & 0x7f7f7f7f */
		l.or	r6, r8, r13		/* t1 = x | 0x7f7f7f7f */
		l.add	r5, r7, r13		/* t2 = t0 + 0x7f7f7f7f */
		l.or	r4, r5, r6		/* t3 = t2 | t1 */
		l.xori	r4, r4, -1		/* t3 = ~t3 */

		l.fl1	r5, r4			/* find last bit set */
		l.ori	r6, r0, 32		/* bits per word */
		l.sub	r7, r6, r5		/* cvt to leading zeros */
		l.srli	r8, r7, 3		/* shift to byte count */

Ldone:
		l.add	r12, r12, r8		/* r12 contains end pointer */

		/* NOTE: Keep it so this function returns the end pointer
		   in r12, so we can it use from other str* calls (strcat
		   comes to mind */

		l.sub	r11, r12, r3		/* length = end - start */
		l.jr	lr			/* return */
		l.nop				/* -- delay slot -- */
END(strlen)
/*----------------------------------------------------------------------*/
