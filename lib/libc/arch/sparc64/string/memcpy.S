/*	$NetBSD: memcpy.S,v 1.2 2001/08/01 05:52:12 eeh Exp $	*/

/*
 * Copyright (c) 2001	Eduardo E. Horvath
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
 */

#include <machine/asm.h>
#ifndef _LOCORE
#define _LOCORE
#endif
#include <machine/ctlreg.h>
#include <machine/frame.h>
#include <machine/psl.h>

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: memcpy.S,v 1.2 2001/08/01 05:52:12 eeh Exp $")
#endif  /* LIBC_SCCS and not lint */

#define	EMPTY	nop
#define	NOTREACHED	ta	1

#define	BCOPY_SMALL	16
#define	BLOCK_SIZE	64

#if 0
#define ASI_STORE	ASI_BLK_COMMIT_P
#else
#define ASI_STORE	ASI_BLK_P
#endif

#if 1
/*
 * kernel bcopy/memcpy
 * Assumes regions do not overlap; has no useful return value.
 *
 * Must not use %g7 (see copyin/copyout above).
 */
ENTRY(memcpy) /* dest, src, size */
	/*
	 * Swap args for bcopy.  Gcc generates calls to memcpy for
	 * structure assignments.
	 */
	mov	%o0, %o3
	mov	%o1, %o0
	mov	%o3, %o1
#endif
ENTRY(bcopy) /* src, dest, size */
#ifdef DEBUG
	set	pmapdebug, %o4
	ld	[%o4], %o4
	btst	0x80, %o4	! PDB_COPY
	bz,pt	%icc, 3f
	 nop
	save	%sp, -CC64FSZ, %sp
	mov	%i0, %o1
	set	2f, %o0
	mov	%i1, %o2
	call	printf
	 mov	%i2, %o3
!	ta	1; nop
	restore
	.data
2:	.asciz	"bcopy(%p->%p,%x)\n"
	_ALIGN
	.text
3:
#endif
	/*
	 * Check for overlaps and punt.
	 *
	 * If src <= dest <= src+len we have a problem.
	 */

	sub	%o1, %o0, %o3

	cmp	%o3, %o2
	blu,pn	%xcc, Lovbcopy
	 cmp	%o2, BCOPY_SMALL
Lbcopy_start:
	bge,pt	%xcc, 2f	! if >= this many, go be fancy.
	 cmp	%o2, 256

	mov	%o1, %o5	! Save memcpy return value
	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	 EMPTY
0:
	inc	%o0
	ldsb	[%o0 - 1], %o4	!	(++dst)[-1] = *src++;
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	retl
	 mov	%o5, %o0
	NOTREACHED

	/*
	 * Overlapping bcopies -- punt.
	 */
Lovbcopy:

	/*
	 * Since src comes before dst, and the regions might overlap,
	 * we have to do the copy starting at the end and working backwards.
	 *
	 * We could optimize this, but it almost never happens.
	 */
	mov	%o1, %o5	! Retval
	add	%o2, %o0, %o0	! src += len
	add	%o2, %o1, %o1	! dst += len
	
	deccc	%o2
	bl,pn	%xcc, 1f
	 dec	%o0
0:
	dec	%o1
	ldsb	[%o0], %o4
	dec	%o0
	
	deccc	%o2
	bge,pt	%xcc, 0b
	 stb	%o4, [%o1]
1:
	retl
	 mov	%o5, %o0

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
2:
#if 1
	! If it is big enough, use VIS instructions
	bge	Lbcopy_block
	 nop
#endif
Lbcopy_fancy:

	!!
	!! First align the output to a 8-byte entity
	!! 

	save	%sp, -CC64FSZ, %sp
	
	mov	%i0, %o0
	mov	%i1, %o1
	
	mov	%i2, %o2
	btst	1, %o1
	
	bz,pt	%icc, 4f
	 btst	2, %o1
	ldub	[%o0], %o4				! Load 1st byte
	
	deccc	1, %o2
	ble,pn	%xcc, Lbcopy_finish			! XXXX
	 inc	1, %o0
	
	stb	%o4, [%o1]				! Store 1st byte
	inc	1, %o1					! Update address
	btst	2, %o1
4:	
	bz,pt	%icc, 4f
	
	 btst	1, %o0
	bz,a	1f
	 lduh	[%o0], %o4				! Load short

	ldub	[%o0], %o4				! Load bytes
	
	ldub	[%o0+1], %o3
	sllx	%o4, 8, %o4
	or	%o3, %o4, %o4
	
1:	
	deccc	2, %o2
	ble,pn	%xcc, Lbcopy_finish			! XXXX
	 inc	2, %o0
	sth	%o4, [%o1]				! Store 1st short
	
	inc	2, %o1
4:
	btst	4, %o1
	bz,pt	%xcc, 4f
	
	 btst	3, %o0
	bz,a,pt	%xcc, 1f
	 lduw	[%o0], %o4				! Load word -1

	btst	1, %o0
	bz,a,pt	%icc, 2f
	 lduh	[%o0], %o4
	
	ldub	[%o0], %o4
	
	lduh	[%o0+1], %o3
	sllx	%o4, 16, %o4
	or	%o4, %o3, %o4
	
	ldub	[%o0+3], %o3
	sllx	%o4, 8, %o4
	ba,pt	%icc, 1f
	 or	%o4, %o3, %o4
	
2:
	lduh	[%o0+2], %o3
	sllx	%o4, 16, %o4
	or	%o4, %o3, %o4
	
1:	
	deccc	4, %o2
	ble,pn	%xcc, Lbcopy_finish		! XXXX
	 inc	4, %o0
	
	st	%o4, [%o1]				! Store word
	inc	4, %o1
4:
	!!
	!! We are now 32-bit aligned in the dest.
	!!
Lbcopy__common:	

	and	%o0, 7, %o4				! Shift amount
	andn	%o0, 7, %o0				! Source addr
	
	brz,pt	%o4, Lbcopy_noshift8			! No shift version...

	 sllx	%o4, 3, %o4				! In bits
	mov	8<<3, %o3
	
	ldx	[%o0], %l0				! Load word -1
	sub	%o3, %o4, %o3				! Reverse shift
	deccc	16*8, %o2				! Have enough room?
	
	sllx	%l0, %o4, %l0
	bl,pn	%xcc, 2f
	 and	%o3, 0x38, %o3
Lbcopy_unrolled8:

	/*
	 * This is about as close to optimal as you can get, since
	 * the shifts require EU0 and cannot be paired, and you have
	 * 3 dependent operations on the data.
	 */ 

!	ldx	[%o0+0*8], %l0				! Already done
!	sllx	%l0, %o4, %l0				! Already done
	ldx	[%o0+1*8], %l1
	ldx	[%o0+2*8], %l2
	ldx	[%o0+3*8], %l3
	ldx	[%o0+4*8], %l4
	ldx	[%o0+5*8], %l5
	ldx	[%o0+6*8], %l6
#if 1
	ba,pt	%icc, 1f
	 ldx	[%o0+7*8], %l7
	.align	8
1:
	srlx	%l1, %o3, %g1
	inc	8*8, %o0
	
	sllx	%l1, %o4, %l1
	or	%g1, %l0, %o5
	ldx	[%o0+0*8], %l0
	
	stx	%o5, [%o1+0*8]
	srlx	%l2, %o3, %g1

	sllx	%l2, %o4, %l2
	or	%g1, %l1, %o5
	ldx	[%o0+1*8], %l1
	
	stx	%o5, [%o1+1*8]
	srlx	%l3, %o3, %g1
	
	sllx	%l3, %o4, %l3
	or	%g1, %l2, %o5
	ldx	[%o0+2*8], %l2
	
	stx	%o5, [%o1+2*8]
	srlx	%l4, %o3, %g1
	
	sllx	%l4, %o4, %l4	
	or	%g1, %l3, %o5
	ldx	[%o0+3*8], %l3
	
	stx	%o5, [%o1+3*8]
	srlx	%l5, %o3, %g1
	
	sllx	%l5, %o4, %l5
	or	%g1, %l4, %o5
	ldx	[%o0+4*8], %l4
	
	stx	%o5, [%o1+4*8]
	srlx	%l6, %o3, %g1
	
	sllx	%l6, %o4, %l6
	or	%g1, %l5, %o5
	ldx	[%o0+5*8], %l5
	
	stx	%o5, [%o1+5*8]
	srlx	%l7, %o3, %g1
	
	sllx	%l7, %o4, %l7
	or	%g1, %l6, %o5
	ldx	[%o0+6*8], %l6
	
	stx	%o5, [%o1+6*8]
	srlx	%l0, %o3, %g1
	deccc	8*8, %o2				! Have enough room?
	
	sllx	%l0, %o4, %l0				! Next loop
	or	%g1, %l7, %o5
	ldx	[%o0+7*8], %l7
	
	stx	%o5, [%o1+7*8]
	bge,pt	%xcc, 1b
	 inc	8*8, %o1

Lbcopy_unrolled8_cleanup:	
	!!
	!! Finished 8 byte block, unload the regs.
	!! 
	srlx	%l1, %o3, %g1
	inc	7*8, %o0
	
	sllx	%l1, %o4, %l1
	or	%g1, %l0, %o5
		
	stx	%o5, [%o1+0*8]
	srlx	%l2, %o3, %g1
	
	sllx	%l2, %o4, %l2
	or	%g1, %l1, %o5
		
	stx	%o5, [%o1+1*8]
	srlx	%l3, %o3, %g1
	
	sllx	%l3, %o4, %l3
	or	%g1, %l2, %o5
		
	stx	%o5, [%o1+2*8]
	srlx	%l4, %o3, %g1
	
	sllx	%l4, %o4, %l4	
	or	%g1, %l3, %o5
		
	stx	%o5, [%o1+3*8]
	srlx	%l5, %o3, %g1
	
	sllx	%l5, %o4, %l5
	or	%g1, %l4, %o5
		
	stx	%o5, [%o1+4*8]
	srlx	%l6, %o3, %g1
	
	sllx	%l6, %o4, %l6
	or	%g1, %l5, %o5
		
	stx	%o5, [%o1+5*8]
	srlx	%l7, %o3, %g1
	
	sllx	%l7, %o4, %l7
	or	%g1, %l6, %o5
		
	stx	%o5, [%o1+6*8]
	inc	7*8, %o1
	
	mov	%l7, %l0				! Save our unused data
	dec	7*8, %o2
#else
	/*
	 * This version also handles aligned copies at almost the
	 * same speed.  It should take the same number of cycles
	 * as the previous version, but is slightly slower, probably
	 * due to i$ issues.
	 */
	ldx	[%o0+7*8], %l7
	ba,pt	%icc, 1f
	 clr	%g1
	.align 32
1:
	srlx	%l1, %o3, %g1
	bz,pn	%xcc, 3f
	 inc	8*8, %o0

	sllx	%l1, %o4, %l1
	or	%g1, %l0, %o5
	ba,pt	%icc, 4f
	ldx	[%o0+0*8], %l0
	
	nop
3:
	mov	%l0, %o5
	ldx	[%o0+0*8], %l0
	
4:	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+0*8]
	srlx	%l2, %o3, %g1

	sllx	%l2, %o4, %l2
3:	
	or	%g1, %l1, %o5
	ldx	[%o0+1*8], %l1
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+1*8]
	srlx	%l3, %o3, %g1
	
	sllx	%l3, %o4, %l3
3:	
	or	%g1, %l2, %o5
	ldx	[%o0+2*8], %l2
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+2*8]
	srlx	%l4, %o3, %g1
	
	sllx	%l4, %o4, %l4
3:	
	or	%g1, %l3, %o5
	ldx	[%o0+3*8], %l3
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+3*8]
	srlx	%l5, %o3, %g1
	
	sllx	%l5, %o4, %l5
3:	
	or	%g1, %l4, %o5
	ldx	[%o0+4*8], %l4
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+4*8]
	srlx	%l6, %o3, %g1
	
	sllx	%l6, %o4, %l6
3:	
	or	%g1, %l5, %o5
	ldx	[%o0+5*8], %l5
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+5*8]
	srlx	%l7, %o3, %g1
	
	sllx	%l7, %o4, %l7
3:	
	or	%g1, %l6, %o5
	ldx	[%o0+6*8], %l6
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+6*8]
	srlx	%l0, %o3, %g1
	
	sllx	%l0, %o4, %l0				! Next loop
3:	
	or	%g1, %l7, %o5
	ldx	[%o0+7*8], %l7
	deccc	8*8, %o2				! Have enough room?
	
	stx	%o5, [%o1+7*8]
	inc	8*8, %o1
	bge,pt	%xcc, 1b
	 tst	%o4


	!!
	!! Now unload all those regs
	!! 
Lbcopy_unrolled8_cleanup:	
	srlx	%l1, %o3, %g1
	bz,pn	%xcc, 3f
	 inc	7*8, %o0				! Point at the last load

	sllx	%l1, %o4, %l1
	ba,pt	%icc, 4f
	 or	%g1, %l0, %o5
	
3:
	mov	%l0, %o5
	
4:	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+0*8]
	srlx	%l2, %o3, %g1

	sllx	%l2, %o4, %l2
3:	
	or	%g1, %l1, %o5
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+1*8]
	srlx	%l3, %o3, %g1
	
	sllx	%l3, %o4, %l3
3:	
	or	%g1, %l2, %o5
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+2*8]
	srlx	%l4, %o3, %g1
	
	sllx	%l4, %o4, %l4
3:	
	or	%g1, %l3, %o5
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+3*8]
	srlx	%l5, %o3, %g1
	
	sllx	%l5, %o4, %l5
3:	
	or	%g1, %l4, %o5
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+4*8]
	srlx	%l6, %o3, %g1
	
	sllx	%l6, %o4, %l6
3:	
	or	%g1, %l5, %o5
	
	bz,pn	%icc, 3f
	stx	%o5, [%o1+5*8]
	srlx	%l7, %o3, %g1
	
	sllx	%l7, %o4, %l7
3:	
	or	%g1, %l6, %o5
	mov	%l7, %l0				! Shuffle to %l0
	
	stx	%o5, [%o1+6*8]
	or	%g1, %l7, %o5
	dec	7*8, %o2
	
	inc	7*8, %o1				! Point at last store
#endif
2:
	inccc	16*8, %o2
	bz,pn	%icc, Lbcopy_complete
	
	!! Unrolled 8 times
Lbcopy_aligned8:	
!	ldx	[%o0], %l0				! Already done
!	sllx	%l0, %o4, %l0				! Shift high word
	
	 deccc	8, %o2					! Pre-decrement
	bl,pn	%xcc, Lbcopy_finish
1:
	ldx	[%o0+8], %l1				! Load word 0
	inc	8, %o0
	
	srlx	%l1, %o3, %o5
	or	%o5, %l0, %o5				! Combine
	
	stx	%o5, [%o1]				! Store result
	 inc	8, %o1
	
	deccc	8, %o2
	bge,pn	%xcc, 1b
	 sllx	%l1, %o4, %l0	

	btst	7, %o2					! Done?
	bz,pt	%xcc, Lbcopy_complete

	!!
	!! Loadup the last dregs into %l0 and shift it into place
	!! 
	 srlx	%o3, 3, %o5				! # bytes in %l0
	dec	8, %o5					!  - 8
	!! n-8 - (by - 8) -> n - by
	subcc	%o2, %o5, %g0				! # bytes we need
	ble,pt	%icc, Lbcopy_finish
	 nop
	ldx	[%o0+8], %l1				! Need another word
	srlx	%l1, %o3, %l1
	ba,pt	%icc, Lbcopy_finish
	 or	%l0, %l1, %l0				! All loaded up.
	
Lbcopy_noshift8:
	deccc	8*8, %o2				! Have enough room?
	bl,pn	%xcc, 2f
	 nop
	ba,pt	%icc, 1f
	 nop
	.align	32
1:	
	ldx	[%o0+0*8], %l0
	ldx	[%o0+1*8], %l1
	ldx	[%o0+2*8], %l2
	ldx	[%o0+3*8], %l3
	stx	%l0, [%o1+0*8]
	stx	%l1, [%o1+1*8]
	stx	%l2, [%o1+2*8]
	stx	%l3, [%o1+3*8]

	
	ldx	[%o0+4*8], %l4
	ldx	[%o0+5*8], %l5
	ldx	[%o0+6*8], %l6
	ldx	[%o0+7*8], %l7
	inc	8*8, %o0
	stx	%l4, [%o1+4*8]
	stx	%l5, [%o1+5*8]
	deccc	8*8, %o2
	stx	%l6, [%o1+6*8]
	stx	%l7, [%o1+7*8]
	stx	%l2, [%o1+2*8]
	bge,pt	%xcc, 1b
	 inc	8*8, %o1
2:
	inc	8*8, %o2
1:	
	deccc	8, %o2
	bl,pn	%icc, 1f				! < 0 --> sub word
	 nop
	ldx	[%o0], %o5
	inc	8, %o0
	stx	%o5, [%o1]
	bg,pt	%icc, 1b				! Exactly 0 --> done
	 inc	8, %o1
1:
	btst	7, %o2					! Done?
	bz,pt	%xcc, Lbcopy_complete
	 clr	%o4
	ldx	[%o0], %l0
Lbcopy_finish:
	
	brz,pn	%o2, 2f					! 100% complete?
	 cmp	%o2, 8					! Exactly 8 bytes?
	bz,a,pn	%xcc, 2f
	 stx	%l0, [%o1]

	btst	4, %o2					! Word store?
	bz	%xcc, 1f
	 srlx	%l0, 32, %o5				! Shift high word down
	stw	%o5, [%o1]
	inc	4, %o1
	mov	%l0, %o5				! Operate on the low bits
1:
	btst	2, %o2
	mov	%o5, %l0
	bz	1f
	 srlx	%l0, 16, %o5
	
	sth	%o5, [%o1]				! Store short
	inc	2, %o1
	mov	%l0, %o5				! Operate on low bytes
1:
	mov	%o5, %l0
	btst	1, %o2					! Byte aligned?
	bz	2f
	 srlx	%l0, 8, %o5

	stb	%o5, [%o1]				! Store last byte
	inc	1, %o1					! Update address
2:	
Lbcopy_complete:
#if 0
	!!
	!! verify copy success.
	!! 

	mov	%i0, %o2
	mov	%i1, %o4
	mov	%i2, %l4
0:	
	ldub	[%o2], %o1
	inc	%o2
	ldub	[%o4], %o3
	inc	%o4
	cmp	%o3, %o1
	bnz	1f
	 dec	%l4
	brnz	%l4, 0b
	 nop
	ba	2f
	 nop

1:
	set	0f, %o0
	call	printf
	 sub	%i2, %l4, %o5
	set	1f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	call	printf
	 mov	%i2, %o3
	ta	1
	.data
0:	.asciz	"bcopy failed: %x@%p != %x@%p byte %d\n"
1:	.asciz	"bcopy(%p, %p, %lx)\n"
	.align 8
	.text
2:	
#endif
	ret
	 restore %i1, %g0, %o0

#if 1

/*
 * Block copy.  Useful for >256 byte copies.
 *
 * Benchmarking has shown this always seems to be slower than
 * the integer version, so this is disabled.  Maybe someone will
 * figure out why sometime.
 */
	
Lbcopy_block:
#ifdef _KERNEL
/*
 * Kernel:
 *
 * Here we use VIS instructions to do a block clear of a page.
 * But before we can do that we need to save and enable the FPU.
 * The last owner of the FPU registers is fpproc, and
 * fpproc->p_md.md_fpstate is the current fpstate.  If that's not
 * null, call savefpstate() with it to store our current fp state.
 *
 * Next, allocate an aligned fpstate on the stack.  We will properly
 * nest calls on a particular stack so this should not be a problem.
 *
 * Now we grab either curproc (or if we're on the interrupt stack
 * proc0).  We stash its existing fpstate in a local register and
 * put our new fpstate in curproc->p_md.md_fpstate.  We point
 * fpproc at curproc (or proc0) and enable the FPU.
 *
 * If we are ever preempted, our FPU state will be saved in our
 * fpstate.  Then, when we're resumed and we take an FPDISABLED
 * trap, the trap handler will be able to fish our FPU state out
 * of curproc (or proc0).
 *
 * On exiting this routine we undo the damage: restore the original
 * pointer to curproc->p_md.md_fpstate, clear our fpproc, and disable
 * the MMU.
 *
 *
 * Register usage, Kernel only (after save):
 *
 * %i0		src
 * %i1		dest
 * %i2		size
 *
 * %l0		XXXX DEBUG old fpstate
 * %l1		fpproc (hi bits only)
 * %l2		orig fpproc
 * %l3		orig fpstate
 * %l5		curproc
 * %l6		old fpstate
 *
 * Register ussage, Kernel and user:
 *
 * %g1		src (retval for memcpy)
 *
 * %o0		src
 * %o1		dest
 * %o2		end dest
 * %o5		last safe fetchable address
 */

#if 1
	ENABLE_FPU(0)
#else
	save	%sp, -(CC64FSZ+FS_SIZE+BLOCK_SIZE), %sp	! Allocate an fpstate
	sethi	%hi(FPPROC), %l1
	LDPTR	[%l1 + %lo(FPPROC)], %l2		! Load fpproc
	add	%sp, (CC64FSZ+STKB+BLOCK_SIZE-1), %l0	! Calculate pointer to fpstate
	brz,pt	%l2, 1f					! fpproc == NULL?
	 andn	%l0, BLOCK_ALIGN, %l0			! And make it block aligned
	LDPTR	[%l2 + P_FPSTATE], %l3
	brz,pn	%l3, 1f					! Make sure we have an fpstate
	 mov	%l3, %o0
	call	_C_LABEL(savefpstate)			! Save the old fpstate
	 set	EINTSTACK-STKB, %l4			! Are we on intr stack?
	cmp	%sp, %l4
	bgu,pt	%xcc, 1f
	 set	INTSTACK-STKB, %l4
	cmp	%sp, %l4
	blu	%xcc, 1f
0:
	 sethi	%hi(_C_LABEL(proc0)), %l4		! Yes, use proc0
	ba,pt	%xcc, 2f				! XXXX needs to change to CPUs idle proc
	 or	%l4, %lo(_C_LABEL(proc0)), %l5
1:
	sethi	%hi(CURPROC), %l4			! Use curproc
	LDPTR	[%l4 + %lo(CURPROC)], %l5
	brz,pn	%l5, 0b					! If curproc is NULL need to use proc0
	 nop
2:
	LDPTR	[%l5 + P_FPSTATE], %l6			! Save old fpstate
	STPTR	%l0, [%l5 + P_FPSTATE]			! Insert new fpstate
	STPTR	%l5, [%l1 + %lo(FPPROC)]		! Set new fpproc
	wr	%g0, FPRS_FEF, %fprs			! Enable FPU
#endif
	mov	%i0, %o0				! Src addr.
	mov	%i1, %o1				! Store our dest ptr here.
	mov	%i2, %o2				! Len counter
#endif

	!!
	!! First align the output to a 64-bit entity
	!! 

	mov	%o1, %g1				! memcpy retval
	add	%o0, %o2, %o5				! End of source block

	andn	%o0, 7, %o3				! Start of block
	dec	%o5
	fzero	%f0

	andn	%o5, BLOCK_ALIGN, %o5			! Last safe addr.
	ldd	[%o3], %f2				! Load 1st word

	dec	8, %o3					! Move %o3 1 word back
	btst	1, %o1
	bz	4f
	
	 mov	-7, %o4					! Lowest src addr possible
	alignaddr %o0, %o4, %o4				! Base addr for load.

	cmp	%o3, %o4
	be,pt	%xcc, 1f				! Already loaded?
	 mov	%o4, %o3
	fmovd	%f2, %f0				! No. Shift
	ldd	[%o3+8], %f2				! And load
1:	

	faligndata	%f0, %f2, %f4			! Isolate 1st byte

	stda	%f4, [%o1] ASI_FL8_P			! Store 1st byte
	inc	1, %o1					! Update address
	inc	1, %o0
	dec	1, %o2
4:	
	btst	2, %o1
	bz	4f

	 mov	-6, %o4					! Calculate src - 6
	alignaddr %o0, %o4, %o4				! calculate shift mask and dest.

	cmp	%o3, %o4				! Addresses same?
	be,pt	%xcc, 1f
	 mov	%o4, %o3
	fmovd	%f2, %f0				! Shuffle data
	ldd	[%o3+8], %f2				! Load word 0
1:	
	faligndata %f0, %f2, %f4			! Move 1st short low part of f8

	stda	%f4, [%o1] ASI_FL16_P			! Store 1st short
	dec	2, %o2
	inc	2, %o1
	inc	2, %o0
4:
	brz,pn	%o2, Lbcopy_blockfinish			! XXXX

	 btst	4, %o1
	bz	4f

	mov	-4, %o4
	alignaddr %o0, %o4, %o4				! calculate shift mask and dest.

	cmp	%o3, %o4				! Addresses same?
	beq,pt	%xcc, 1f
	 mov	%o4, %o3
	fmovd	%f2, %f0				! Shuffle data
	ldd	[%o3+8], %f2				! Load word 0
1:	
	faligndata %f0, %f2, %f4			! Move 1st short low part of f8

	st	%f5, [%o1]				! Store word
	dec	4, %o2
	inc	4, %o1
	inc	4, %o0
4:
	brz,pn	%o2, Lbcopy_blockfinish			! XXXX
	!!
	!! We are now 32-bit aligned in the dest.
	!!
Lbcopy_block_common:	

	 mov	-0, %o4
	alignaddr %o0, %o4, %o4				! base - shift

	cmp	%o3, %o4				! Addresses same?
	beq,pt	%xcc, 1f
	 mov	%o4, %o3
	fmovd	%f2, %f0				! Shuffle data
	ldd	[%o3+8], %f2				! Load word 0
1:	
	add	%o3, 8, %o0				! now use %o0 for src
	
	!!
	!! Continue until our dest is block aligned
	!! 
Lbcopy_block_aligned8:	
1:
	brz	%o2, Lbcopy_blockfinish
	 btst	BLOCK_ALIGN, %o1			! Block aligned?
	bz	1f
	
	 faligndata %f0, %f2, %f4			! Generate result
	deccc	8, %o2
	ble,pn	%icc, Lbcopy_blockfinish		! Should never happen
	 fmovd	%f4, %f48
	
	std	%f4, [%o1]				! Store result
	inc	8, %o1
	
	fmovd	%f2, %f0
	inc	8, %o0
	ba,pt	%xcc, 1b				! Not yet.
	 ldd	[%o0], %f2				! Load next part
Lbcopy_block_aligned64:	
1:

/*
 * 64-byte aligned -- ready for block operations.
 *
 * Here we have the destination block aligned, but the
 * source pointer may not be.  Sub-word alignment will
 * be handled by faligndata instructions.  But the source
 * can still be potentially aligned to 8 different words
 * in our 64-bit block, so we have 8 different copy routines.
 *
 * Once we figure out our source alignment, we branch
 * to the appropriate copy routine, which sets up the
 * alignment for faligndata and loads (sets) the values
 * into the source registers and does the copy loop.
 *
 * When were down to less than 1 block to store, we
 * exit the copy loop and execute cleanup code.
 *
 * Block loads and stores are not properly interlocked.
 * Stores save one reg/cycle, so you can start overwriting
 * registers the cycle after the store is issued.  
 * 
 * Block loads require a block load to a different register
 * block or a membar #Sync before accessing the loaded
 * data.
 *	
 * Since the faligndata instructions may be offset as far
 * as 7 registers into a block (if you are shifting source 
 * 7 -> dest 0), you need 3 source register blocks for full 
 * performance: one you are copying, one you are loading, 
 * and one for interlocking.  Otherwise, we would need to
 * sprinkle the code with membar #Sync and lose the advantage
 * of running faligndata in parallel with block stores.  This 
 * means we are fetching a full 128 bytes ahead of the stores.  
 * We need to make sure the prefetch does not inadvertently 
 * cross a page boundary and fault on data that we will never 
 * store.
 *
 */
#if 1
	and	%o0, BLOCK_ALIGN, %o3
	srax	%o3, 3, %o3				! Isolate the offset

	brz	%o3, L100				! 0->0
	 btst	4, %o3
	bnz	%xcc, 4f
	 btst	2, %o3
	bnz	%xcc, 2f
	 btst	1, %o3
	ba,pt	%xcc, L101				! 0->1
	 nop	/* XXX spitfire bug */
2:
	bz	%xcc, L102				! 0->2
	 nop
	ba,pt	%xcc, L103				! 0->3
	 nop	/* XXX spitfire bug */
4:	
	bnz	%xcc, 2f
	 btst	1, %o3
	bz	%xcc, L104				! 0->4
	 nop
	ba,pt	%xcc, L105				! 0->5
	 nop	/* XXX spitfire bug */
2:
	bz	%xcc, L106				! 0->6
	 nop
	ba,pt	%xcc, L107				! 0->7
	 nop	/* XXX spitfire bug */
#else

	!!
	!! Isolate the word offset, which just happens to be
	!! the slot in our jump table.
	!!
	!! This is 6 insns, most of which cannot be paired,
	!! which is about the same as the above version.
	!!
	rd	%pc, %o4
1:	
	and	%o0, 0x31, %o3
	add	%o3, (Lbcopy_block_jmp - 1b), %o3
	jmpl	%o4 + %o3, %g0
	 nop

	!!
	!! Jump table
	!!
	
Lbcopy_block_jmp:
	ba,a,pt	%xcc, L100
	 nop
	ba,a,pt	%xcc, L101
	 nop
	ba,a,pt	%xcc, L102
	 nop
	ba,a,pt	%xcc, L103
	 nop
	ba,a,pt	%xcc, L104
	 nop
	ba,a,pt	%xcc, L105
	 nop
	ba,a,pt	%xcc, L106
	 nop
	ba,a,pt	%xcc, L107
	 nop
#endif

	!!
	!! Source is block aligned.
	!!
	!! Just load a block and go.
	!!
L100:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L100"
	.align	8
2:	
#endif
	fmovd	%f0 , %f62
	ldda	[%o0] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %o0
	cmp	%o0, %o5
	bleu,a,pn	%icc, 3f
	 ldda	[%o0] ASI_BLK_P, %f16
	ba,pt	%icc, 3f
	 membar #Sync
	
	.align	32					! ICache align.
3:
	faligndata	%f62, %f0, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f0, %f2, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f2, %f4, %f36
	cmp	%o0, %o5
	faligndata	%f4, %f6, %f38
	faligndata	%f6, %f8, %f40
	faligndata	%f8, %f10, %f42
	faligndata	%f10, %f12, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f12, %f14, %f46
	
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:	
	stda	%f32, [%o1] ASI_STORE
	faligndata	%f14, %f16, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f16, %f18, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f18, %f20, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f20, %f22, %f38
	cmp	%o0, %o5
	faligndata	%f22, %f24, %f40
	faligndata	%f24, %f26, %f42
	faligndata	%f26, %f28, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f28, %f30, %f46
	
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	stda	%f32, [%o1] ASI_STORE
	faligndata	%f30, %f48, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f48, %f50, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f50, %f52, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f52, %f54, %f38
	cmp	%o0, %o5
	faligndata	%f54, %f56, %f40
	faligndata	%f56, %f58, %f42
	faligndata	%f58, %f60, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f60, %f62, %f46
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16			! Increment is at top
	membar	#Sync
2:	
	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1
	
	!!
	!! Source at BLOCK_ALIGN+8
	!!
	!! We need to load almost 1 complete block by hand.
	!! 
L101:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L101"
	.align	8
2:	
#endif
!	fmovd	%f0, %f0				! Hoist fmovd
	ldd	[%o0], %f2
	inc	8, %o0
	ldd	[%o0], %f4
	inc	8, %o0
	ldd	[%o0], %f6
	inc	8, %o0
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 3f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
3:	
	faligndata	%f0, %f2, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f2, %f4, %f34
	cmp	%o0, %o5
	faligndata	%f4, %f6, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f6, %f8, %f38
	faligndata	%f8, %f10, %f40
	faligndata	%f10, %f12, %f42
	faligndata	%f12, %f14, %f44
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f14, %f16, %f46

	stda	%f32, [%o1] ASI_STORE
	
	faligndata	%f16, %f18, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f20, %f22, %f36
	cmp	%o0, %o5
	faligndata	%f22, %f24, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f24, %f26, %f40
	faligndata	%f26, %f28, %f42
	faligndata	%f28, %f30, %f44
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f30, %f48, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f48, %f50, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f50, %f52, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f52, %f54, %f36
	cmp	%o0, %o5
	faligndata	%f54, %f56, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f56, %f58, %f40
	faligndata	%f58, %f60, %f42
	faligndata	%f60, %f62, %f44
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f62, %f0, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1

	!!
	!! Source at BLOCK_ALIGN+16
	!!
	!! We need to load 6 doubles by hand.
	!! 
L102:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L102"
	.align	8
2:	
#endif
	ldd	[%o0], %f4
	inc	8, %o0
	fmovd	%f0, %f2				! Hoist fmovd
	ldd	[%o0], %f6
	inc	8, %o0
	
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 3f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
3:	
	faligndata	%f2, %f4, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f4, %f6, %f34
	cmp	%o0, %o5
	faligndata	%f6, %f8, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f8, %f10, %f38
	faligndata	%f10, %f12, %f40
	faligndata	%f12, %f14, %f42
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f44

	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f16, %f18, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f18, %f20, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f20, %f22, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f22, %f24, %f36
	cmp	%o0, %o5
	faligndata	%f24, %f26, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f26, %f28, %f40
	faligndata	%f28, %f30, %f42
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	faligndata	%f30, %f48, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f48, %f50, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f50, %f52, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f52, %f54, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f54, %f56, %f36
	cmp	%o0, %o5
	faligndata	%f56, %f58, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f58, %f60, %f40
	faligndata	%f60, %f62, %f42
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	faligndata	%f62, %f0, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f0, %f2, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1
	
	!!
	!! Source at BLOCK_ALIGN+24
	!!
	!! We need to load 5 doubles by hand.
	!! 
L103:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L103"
	.align	8
2:	
#endif
	fmovd	%f0, %f4
	ldd	[%o0], %f6
	inc	8, %o0
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0

	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f4, %f6, %f32
	cmp	%o0, %o5
	faligndata	%f6, %f8, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f8, %f10, %f36
	faligndata	%f10, %f12, %f38
	faligndata	%f12, %f14, %f40
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f16, %f18, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f18, %f20, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f20, %f22, %f32
	cmp	%o0, %o5
	faligndata	%f22, %f24, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f24, %f26, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f26, %f28, %f38
	faligndata	%f28, %f30, %f40
	ble,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	faligndata	%f30, %f48, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f48, %f50, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f50, %f52, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f52, %f54, %f32
	cmp	%o0, %o5
	faligndata	%f54, %f56, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f56, %f58, %f36
	faligndata	%f58, %f60, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f60, %f62, %f40
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	faligndata	%f62, %f0, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f0, %f2, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f2, %f4, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1

	!!
	!! Source at BLOCK_ALIGN+32
	!!
	!! We need to load 4 doubles by hand.
	!! 
L104:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L104"
	.align	8
2:	
#endif
	fmovd	%f0, %f6
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f6, %f8, %f32
	cmp	%o0, %o5
	faligndata	%f8, %f10, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f10, %f12, %f36
	faligndata	%f12, %f14, %f38
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f40
	faligndata	%f16, %f18, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f20, %f22, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f22, %f24, %f32
	cmp	%o0, %o5
	faligndata	%f24, %f26, %f34
	faligndata	%f26, %f28, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f28, %f30, %f38
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	faligndata	%f30, %f48, %f40
	dec	BLOCK_SIZE, %o2
	faligndata	%f48, %f50, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f50, %f52, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f52, %f54, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f54, %f56, %f32
	cmp	%o0, %o5
	faligndata	%f56, %f58, %f34
	faligndata	%f58, %f60, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f60, %f62, %f38
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	faligndata	%f62, %f0, %f40
	dec	BLOCK_SIZE, %o2
	faligndata	%f0, %f2, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f2, %f4, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f4, %f6, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1

	!!
	!! Source at BLOCK_ALIGN+40
	!!
	!! We need to load 3 doubles by hand.
	!! 
L105:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L105"
	.align	8
2:	
#endif
	fmovd	%f0, %f8
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f8, %f10, %f32
	cmp	%o0, %o5
	faligndata	%f10, %f12, %f34
	faligndata	%f12, %f14, %f36
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f16, %f18, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f42
	faligndata	%f20, %f22, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f22, %f24, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f24, %f26, %f32
	cmp	%o0, %o5
	faligndata	%f26, %f28, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f28, %f30, %f36
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	faligndata	%f30, %f48, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f48, %f50, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f50, %f52, %f42
	faligndata	%f52, %f54, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f54, %f56, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f56, %f58, %f32
	cmp	%o0, %o5
	faligndata	%f58, %f60, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f60, %f62, %f36
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:
	faligndata	%f62, %f0, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f0, %f2, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f2, %f4, %f42
	faligndata	%f4, %f6, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f6, %f8, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1


	!!
	!! Source at BLOCK_ALIGN+48
	!!
	!! We need to load 2 doubles by hand.
	!! 
L106:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L106"
	.align	8
2:	
#endif
	fmovd	%f0, %f10
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f10, %f12, %f32
	cmp	%o0, %o5
	faligndata	%f12, %f14, %f34
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f16, %f18, %f38
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f40
	faligndata	%f20, %f22, %f42
	faligndata	%f22, %f24, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f24, %f26, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f26, %f28, %f32
	cmp	%o0, %o5
	faligndata	%f28, %f30, %f34
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	faligndata	%f30, %f48, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f48, %f50, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f50, %f52, %f40
	faligndata	%f52, %f54, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f54, %f56, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f56, %f58, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f58, %f60, %f32
	cmp	%o0, %o5
	faligndata	%f60, %f62, %f34
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:
	faligndata	%f62, %f0, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f0, %f2, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f2, %f4, %f40
	faligndata	%f4, %f6, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f6, %f8, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f8, %f10, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1


	!!
	!! Source at BLOCK_ALIGN+56
	!!
	!! We need to load 1 double by hand.
	!! 
L107:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L107"
	.align	8
2:	
#endif
	fmovd	%f0, %f12
	ldd	[%o0], %f14
	inc	8, %o0

	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f12, %f14, %f32
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f16, %f18, %f36
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f38
	faligndata	%f20, %f22, %f40
	faligndata	%f22, %f24, %f42
	faligndata	%f24, %f26, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f26, %f28, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f28, %f30, %f32
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	faligndata	%f30, %f48, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f48, %f50, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f50, %f52, %f38
	faligndata	%f52, %f54, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f54, %f56, %f42
	faligndata	%f56, %f58, %f44
	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f58, %f60, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f60, %f62, %f32
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:
	faligndata	%f62, %f0, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f0, %f2, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f2, %f4, %f38
	faligndata	%f4, %f6, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f6, %f8, %f42
	faligndata	%f8, %f10, %f44

	brlez,pn	%o2, Lbcopy_blockdone
	 faligndata	%f10, %f12, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1
	
Lbcopy_blockdone:
	inc	BLOCK_SIZE, %o2				! Fixup our overcommit
	membar	#Sync					! Finish any pending loads
#define	FINISH_REG(f)				\
	deccc	8, %o2;				\
	bl,a	Lbcopy_blockfinish;		\
	 fmovd	f, %f48;			\
	std	f, [%o1];			\
	inc	8, %o1

	FINISH_REG(%f32)
	FINISH_REG(%f34)
	FINISH_REG(%f36)
	FINISH_REG(%f38)
	FINISH_REG(%f40)
	FINISH_REG(%f42)
	FINISH_REG(%f44)
	FINISH_REG(%f46)
	FINISH_REG(%f48)
#undef FINISH_REG
	!! 
	!! The low 3 bits have the sub-word bits needed to be
	!! stored [because (x-8)&0x7 == x].
	!!
Lbcopy_blockfinish:
	brz,pn	%o2, 2f					! 100% complete?
	 fmovd	%f48, %f4
	cmp	%o2, 8					! Exactly 8 bytes?
	bz,a,pn	%xcc, 2f
	 std	%f4, [%o1]

	btst	4, %o2					! Word store?
	bz	%xcc, 1f
	 nop
	st	%f4, [%o1]
	inc	4, %o1
1:
	btst	2, %o2
	fzero	%f0
	bz	1f

	 mov	-6, %o4
	alignaddr %o1, %o4, %g0

	faligndata %f0, %f4, %f8
	
	stda	%f8, [%o1] ASI_FL16_P			! Store short
	inc	2, %o1
1:
	btst	1, %o2					! Byte aligned?
	bz	2f

	 mov	-7, %o0					! Calculate dest - 7
	alignaddr %o1, %o0, %g0				! Calculate shift mask and dest.

	faligndata %f0, %f4, %f8			! Move 1st byte to low part of f8

	stda	%f8, [%o1] ASI_FL8_P			! Store 1st byte
	inc	1, %o1					! Update address
2:
	membar	#Sync
#if 0
	!!
	!! verify copy success.
	!! 

	mov	%i0, %o2
	mov	%i1, %o4
	mov	%i2, %l4
0:	
	ldub	[%o2], %o1
	inc	%o2
	ldub	[%o4], %o3
	inc	%o4
	cmp	%o3, %o1
	bnz	1f
	 dec	%l4
	brnz	%l4, 0b
	 nop
	ba	2f
	 nop

1:
	set	block_disable, %o0
	stx	%o0, [%o0]
	
	set	0f, %o0
	call	prom_printf
	 sub	%i2, %l4, %o5
	set	1f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	call	prom_printf
	 mov	%i2, %o3
	ta	1
	.data
	_ALIGN
block_disable:	.xword	0
0:	.asciz	"bcopy failed: %x@%p != %x@%p byte %d\r\n"
1:	.asciz	"bcopy(%p, %p, %lx)\r\n"
	_ALIGN
	.text
2:	
#endif
#ifdef _KERNEL		

	set 1f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	call	printf
	mov	%i2, %o3
	
	.data
	_ALIGN
1:	.asciz "block exit (%p, %p, %d)\n"
	_ALIGN
	.text
/*
 * Weve saved our possible fpstate, now disable the fpu
 * and continue with life.
 */
#if 1
	RESTORE_FPU
#else
#ifdef DEBUG
	LDPTR	[%l1 + %lo(FPPROC)], %l7
	cmp	%l7, %l5
!	tnz	1		! fpproc has changed!
	LDPTR	[%l5 + P_FPSTATE], %l7
	cmp	%l7, %l0
	tnz	1		! fpstate has changed!
#endif
	andcc	%l2, %l3, %g0				! If (fpproc && fpstate)
	STPTR	%l2, [%l1 + %lo(FPPROC)]		! Restore old fproc
	bz,pt	%xcc, 1f				! Skip if no fpstate
	 STPTR	%l6, [%l5 + P_FPSTATE]			! Restore old fpstate
	
	call	_C_LABEL(loadfpstate)			! Re-load orig fpstate
	 mov	%l3, %o0
1:
#endif
	set 1f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	call	printf
	mov	%i2, %o3
	
	.data
	_ALIGN
1:	.asciz "block done (%p, %p, %d)\n"
	_ALIGN
	.text

	
	ret
	 restore	%g1, 0, %o0			! Return DEST for memcpy
#endif
	retl
	 mov	%g1, %o0
#endif


