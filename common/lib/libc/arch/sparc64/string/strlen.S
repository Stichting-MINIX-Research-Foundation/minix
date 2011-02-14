/*	$NetBSD: strlen.S,v 1.1 2005/12/20 19:28:50 christos Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Eduardo Horvath for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */



#include <machine/asm.h>
#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: strlen.S,v 1.1 2005/12/20 19:28:50 christos Exp $")
#endif /* LIBC_SCCS and not lint */
	
/* The algorithm here uses the following techniques:
 *
 * 1) Given a word 'x', we can test to see if it contains any 0 bytes
 *    by subtracting 0x01010101, and seeing if any of the high bits of each
 *    byte changed from 0 to 1. This works because the least significant
 *    0 byte must have had no incoming carry (otherwise it's not the least
 *    significant), so it is 0x00 - 0x01 == 0xff. For all other
 *    byte values, either they have the high bit set initially, or when
 *    1 is subtracted you get a value in the range 0x00-0x7f, none of which
 *    have their high bit set. The expression here is
 *    (x + 0xfefefeff) & ~(x | 0x7f7f7f7f), which gives 0x00000000 when
 *    there were no 0x00 bytes in the word.
 *
 * 2) Now just hunt for the first byte that's 0x00 in 'x'.
 *
 *    This is from the book 'The PowerPC Compiler Writer's Guide',
 *    by Steve Hoxey, Faraydon Karim, Bill Hay and Hank Warren.
 */

ENTRY(strlen)
	/*
	 * Calculate address for and load the first xword.
	 */
	andn	%o0, 0x7, %o1
	ldx	[%o1], %g1

	/*
	 * Now prepare some constants while the data arrives...
	 */
	sethi	%hi(0xfefefefe), %o3
	sethi	%hi(0x7f7f7f7f), %o2
	
	or	%o3, %lo(0xfefefefe), %o3
	or	%o2, %lo(0x7f7f7f7f), %o2
	
	sllx	%o3, 32, %o5
	andcc	%o0, 0x7, %g5	! Hoisted from below to fill a slot
	
	sllx	%o2, 32, %o4
	or	%o3, %o5, %o3
	
	sll	%g5, 3,	%g5	! Convert to bytes. hoisted
	or	%o2, %o4, %o2
	
	inc	%o3
	neg	%g5		! hoisted

	/*
	 * Mask off the leading bits:
	 *
	 * if (ptr & 0x7)
	 *	mask = -1 << (64 - ((ptr & 0x7) << 3));
	 */
	
!	andcc	%o0, 0x7, %g5	! Hoisted above
	bz,pt	%icc, 0f
	
	
!	 sll	%g5, 3,	%g5	! Convert to bytes. Also hoisted

!	neg	%g5		! Hoisted
	
	 add	%g5, 64, %g5
	mov	-1, %o4
	
	sllx	%o4, %g5, %o4

	or	%o4, %g1, %g1	! Make leading bytes != 0
	
0:
	or	%g1, %o2, %o5	! Do step 1 -- use or/andn instead of nor/and
	add	%g1, %o3, %g5
	
	inc	8, %o1		! Point to next word
	andncc	%g5, %o5, %g0
	bz,a,pt	%xcc, 0b
	 ldx	[%o1], %g1
	
	mov	-1, %o4
	dec	8, %o1
	
	sllx	%o4, 64-8, %o5
	
	btst	%g1, %o5	! Check high byte
	bz	%xcc,0f
	 srlx	%o5, 8, %o5

	inc	%o1
	btst	%g1, %o5	! Check 2nd byte
	bz	%xcc,0f
	 srlx	%o5, 8, %o5
	
	inc	%o1
	btst	%g1, %o5	! Check 3rd byte
	bz	%xcc,0f
	 srlx	%o5, 8, %o5

	inc	%o1
	btst	%g1, %o5	! Check 4th byte
	bz	%xcc,0f
	 srlx	%o5, 8, %o5
			
	inc	%o1
	btst	%g1, %o5	! Check 5th byte
	bz	%xcc,0f
	 srlx	%o5, 8, %o5
			
	inc	%o1
	btst	%g1, %o5	! Check 6th byte
	bz	%xcc,0f
	 srlx	%o5, 8, %o5
			
	inc	%o1
	btst	%g1, %o5	! Check 7th byte
	bz	%xcc,0f
	 nop

	inc	%o1
0:
	retl
	 sub	%o1, %o0, %o0	! return length (ptr - (origptr+1))
