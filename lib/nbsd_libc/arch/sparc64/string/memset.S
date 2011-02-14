/*	$NetBSD: memset.S,v 1.4 2001/08/02 01:17:28 eeh Exp $	*/

/*
 * Copyright (c) 2001, Eduardo E. Horvath
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
#ifndef _LOCORE
#define _LOCORE
#endif
#include <machine/ctlreg.h>
#include <machine/frame.h>
#include <machine/psl.h>

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: memset.S,v 1.4 2001/08/02 01:17:28 eeh Exp $")
#endif  /* LIBC_SCCS and not lint */

/*
 * bzero(addr, len)
 *
 * We want to use VIS instructions if we're clearing out more than
 * 256 bytes, but to do that we need to properly save and restore the
 * FP registers.  Unfortunately the code to do that in the kernel needs
 * to keep track of the current owner of the FPU, hence the different
 * code.
 *
 * XXXXX To produce more efficient code, we do not allow lengths
 * greater than 0x80000000000000000, which are negative numbers.
 * This should not really be an issue since the VA hole should
 * cause any such ranges to fail anyway.
 */
ENTRY(bzero)
	! %o0 = addr, %o1 = len
	mov	%o1, %o2
	clr	%o1			! Initialize our pattern
/*
 * memset(addr, c, len)
 *
 */
ENTRY(memset)
	! %o0 = addr, %o1 = pattern, %o2 = len
	mov	%o0, %o4		! Save original pointer

Lbzero_internal:
	btst	7, %o0			! Word aligned?
	bz,pn	%xcc, 0f
	 nop
	inc	%o0
	deccc	%o2			! Store up to 7 bytes
	bge,a,pt	%xcc, Lbzero_internal
	 stb	%o1, [%o0 - 1]

	retl				! Duplicate Lbzero_done
	 mov	%o4, %o0
0:
	/*
	 * Duplicate the pattern so it fills 64-bits.
	 */
	andcc	%o1, 0x0ff, %o1		! No need to extend zero
	bz,pt	%icc, 1f
	 sllx	%o1, 8, %o3		! sigh.  all dependent insns.
	or	%o1, %o3, %o1
	sllx	%o1, 16, %o3
	or	%o1, %o3, %o1
	sllx	%o1, 32, %o3
	 or	%o1, %o3, %o1
1:	
#if 1
	!! Now we are 64-bit aligned
	cmp	%o2, 256		! Use block clear if len > 256
	bge,pt	%xcc, Lbzero_block	! use block store insns
#endif	
	 deccc	8, %o2
Lbzero_longs:
	bl,pn	%xcc, Lbzero_cleanup	! Less than 8 bytes left
	 nop
3:	
	inc	8, %o0
	deccc	8, %o2
	bge,pt	%xcc, 3b
	 stx	%o1, [%o0 - 8]		! Do 1 longword at a time

	/*
	 * Len is in [-8..-1] where -8 => done, -7 => 1 byte to zero,
	 * -6 => two bytes, etc.  Mop up this remainder, if any.
	 */
Lbzero_cleanup:	
	btst	4, %o2
	bz,pt	%xcc, 5f		! if (len & 4) {
	 nop
	stw	%o1, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
5:	
	btst	2, %o2
	bz,pt	%xcc, 7f		! if (len & 2) {
	 nop
	sth	%o1, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
7:	
	btst	1, %o2
	bnz,a	%icc, Lbzero_done	! if (len & 1)
	 stb	%o1, [%o0]		!	*addr = 0;
Lbzero_done:
	retl
	 mov	%o4, %o0		! Restore ponter for memset (ugh)

#if 1	
Lbzero_block:
/*
 * Userland:
 *
 * Floating point registers are volatile.  What luck.
 *
 * See locore.s for the kernel version.
 *
 */	
!	wr	%g0, FPRS_FEF, %fprs			! Enable FPU

	!! We are now 8-byte aligned.  We need to become 64-byte aligned.
	btst	63, %o0
	bz,pt	%xcc, 2f
	 nop
1:	
	stx	%o1, [%o0]
	inc	8, %o0
	btst	63, %o0
	bnz,pt	%xcc, 1b
	 dec	8, %o2

2:
	brz	%o1, 3f					! Skip the memory op
	 fzero	%f0					! for bzero
	
	stx	%o1, [%o0]				! Flush this puppy to RAM
	membar	#StoreLoad
	ldd	[%o0], %f0
3:	
	fmovd	%f0, %f2				! Duplicate the pattern
	fmovd	%f0, %f4
	fmovd	%f0, %f6
	fmovd	%f0, %f8
	fmovd	%f0, %f10
	fmovd	%f0, %f12
	fmovd	%f0, %f14
	
	!! Remember: we were 8 bytes too far
	dec	56, %o2					! Go one iteration too far
5:
	stda	%f0, [%o0] ASI_BLK_P			! Store 64 bytes
	deccc	64, %o2
	bg,pn	%xcc, 5b
	 inc	64, %o0
	
	membar	#Sync
/*
 * Now we're done we need to load the FPU state from where
 * we put it.
 */
	ba,pt	%xcc, Lbzero_longs	! Finish up the remainder
	 inccc	56, %o2		! Restore the count
#endif
