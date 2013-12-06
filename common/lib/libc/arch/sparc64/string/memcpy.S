/*	$NetBSD: memcpy.S,v 1.2 2013/03/17 02:13:10 christos Exp $	*/

/*
 * Copyright (c) 1996-2002 Eduardo Horvath
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include "strmacros.h"
#if defined(LIBC_SCCS) && !defined(lint)
RCSID("$NetBSD: memcpy.S,v 1.2 2013/03/17 02:13:10 christos Exp $")
#endif  /* LIBC_SCCS and not lint */

/*
 * memcpy
 * Assumes regions do not overlap;
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
#if !defined(_KERNEL) || defined(_RUMPKERNEL)
ENTRY(bcopy) /* src, dest, size */
#endif
#ifdef DEBUG
#if defined(_KERNEL) && !defined(_RUMPKERNEL)
	set	pmapdebug, %o4
	ld	[%o4], %o4
	btst	0x80, %o4	! PDB_COPY
	bz,pt	%icc, 3f
	 nop
#endif
	save	%sp, -CC64FSZ, %sp
	mov	%i0, %o1
	set	2f, %o0
	mov	%i1, %o2
	call	printf
	 mov	%i2, %o3
!	ta	1; nop
	restore
	.data
2:	.asciz	"memcpy(%p<-%p,%x)\n"
	_ALIGN
	.text
3:
#endif

	cmp	%o2, BCOPY_SMALL

Lmemcpy_start:
	bge,pt	CCCR, 2f	! if >= this many, go be fancy.
	 cmp	%o2, 256

	mov	%o1, %o5	! Save memcpy return value
	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	 .empty
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
	 * Plenty of data to copy, so try to do it optimally.
	 */
2:
#ifdef USE_BLOCK_STORE_LOAD
	! If it is big enough, use VIS instructions
	bge	Lmemcpy_block
	 nop
#endif /* USE_BLOCK_STORE_LOAD */
Lmemcpy_fancy:

	!!
	!! First align the output to a 8-byte entity
	!! 

	save	%sp, -CC64FSZ, %sp
	
	mov	%i0, %l0
	mov	%i1, %l1
	
	mov	%i2, %l2
	btst	1, %l1
	
	bz,pt	%icc, 4f
	 btst	2, %l1
	ldub	[%l0], %l4				! Load 1st byte
	
	deccc	1, %l2
	ble,pn	CCCR, Lmemcpy_finish			! XXXX
	 inc	1, %l0
	
	stb	%l4, [%l1]				! Store 1st byte
	inc	1, %l1					! Update address
	btst	2, %l1
4:	
	bz,pt	%icc, 4f
	
	 btst	1, %l0
	bz,a	1f
	 lduh	[%l0], %l4				! Load short

	ldub	[%l0], %l4				! Load bytes
	
	ldub	[%l0+1], %l3
	sllx	%l4, 8, %l4
	or	%l3, %l4, %l4
	
1:	
	deccc	2, %l2
	ble,pn	CCCR, Lmemcpy_finish			! XXXX
	 inc	2, %l0
	sth	%l4, [%l1]				! Store 1st short
	
	inc	2, %l1
4:
	btst	4, %l1
	bz,pt	CCCR, 4f
	
	 btst	3, %l0
	bz,a,pt	CCCR, 1f
	 lduw	[%l0], %l4				! Load word -1

	btst	1, %l0
	bz,a,pt	%icc, 2f
	 lduh	[%l0], %l4
	
	ldub	[%l0], %l4
	
	lduh	[%l0+1], %l3
	sllx	%l4, 16, %l4
	or	%l4, %l3, %l4
	
	ldub	[%l0+3], %l3
	sllx	%l4, 8, %l4
	ba,pt	%icc, 1f
	 or	%l4, %l3, %l4
	
2:
	lduh	[%l0+2], %l3
	sllx	%l4, 16, %l4
	or	%l4, %l3, %l4
	
1:	
	deccc	4, %l2
	ble,pn	CCCR, Lmemcpy_finish		! XXXX
	 inc	4, %l0
	
	st	%l4, [%l1]				! Store word
	inc	4, %l1
4:
	!!
	!! We are now 32-bit aligned in the dest.
	!!
Lmemcpy_common:	

	and	%l0, 7, %l4				! Shift amount
	andn	%l0, 7, %l0				! Source addr
	
	brz,pt	%l4, Lmemcpy_noshift8			! No shift version...

	 sllx	%l4, 3, %l4				! In bits
	mov	8<<3, %l3
	
	ldx	[%l0], %o0				! Load word -1
	sub	%l3, %l4, %l3				! Reverse shift
	deccc	12*8, %l2				! Have enough room?
	
	sllx	%o0, %l4, %o0
	bl,pn	CCCR, 2f
	 and	%l3, 0x38, %l3
Lmemcpy_unrolled8:

	/*
	 * This is about as close to optimal as you can get, since
	 * the shifts require EU0 and cannot be paired, and you have
	 * 3 dependent operations on the data.
	 */ 

!	ldx	[%l0+0*8], %o0				! Already done
!	sllx	%o0, %l4, %o0				! Already done
	ldx	[%l0+1*8], %o1
	ldx	[%l0+2*8], %o2
	ldx	[%l0+3*8], %o3
	ldx	[%l0+4*8], %o4
	ba,pt	%icc, 1f
	 ldx	[%l0+5*8], %o5
	.align	8
1:
	srlx	%o1, %l3, %g1
	inc	6*8, %l0
	
	sllx	%o1, %l4, %o1
	or	%g1, %o0, %g6
	ldx	[%l0+0*8], %o0
	
	stx	%g6, [%l1+0*8]
	srlx	%o2, %l3, %g1

	sllx	%o2, %l4, %o2
	or	%g1, %o1, %g6
	ldx	[%l0+1*8], %o1
	
	stx	%g6, [%l1+1*8]
	srlx	%o3, %l3, %g1
	
	sllx	%o3, %l4, %o3
	or	%g1, %o2, %g6
	ldx	[%l0+2*8], %o2
	
	stx	%g6, [%l1+2*8]
	srlx	%o4, %l3, %g1
	
	sllx	%o4, %l4, %o4	
	or	%g1, %o3, %g6
	ldx	[%l0+3*8], %o3
	
	stx	%g6, [%l1+3*8]
	srlx	%o5, %l3, %g1
	
	sllx	%o5, %l4, %o5
	or	%g1, %o4, %g6
	ldx	[%l0+4*8], %o4

	stx	%g6, [%l1+4*8]
	srlx	%o0, %l3, %g1
	deccc	6*8, %l2				! Have enough room?

	sllx	%o0, %l4, %o0				! Next loop
	or	%g1, %o5, %g6
	ldx	[%l0+5*8], %o5
	
	stx	%g6, [%l1+5*8]
	bge,pt	CCCR, 1b
	 inc	6*8, %l1

Lmemcpy_unrolled8_cleanup:	
	!!
	!! Finished 8 byte block, unload the regs.
	!! 
	srlx	%o1, %l3, %g1
	inc	5*8, %l0
	
	sllx	%o1, %l4, %o1
	or	%g1, %o0, %g6
		
	stx	%g6, [%l1+0*8]
	srlx	%o2, %l3, %g1
	
	sllx	%o2, %l4, %o2
	or	%g1, %o1, %g6
		
	stx	%g6, [%l1+1*8]
	srlx	%o3, %l3, %g1
	
	sllx	%o3, %l4, %o3
	or	%g1, %o2, %g6
		
	stx	%g6, [%l1+2*8]
	srlx	%o4, %l3, %g1
	
	sllx	%o4, %l4, %o4	
	or	%g1, %o3, %g6
		
	stx	%g6, [%l1+3*8]
	srlx	%o5, %l3, %g1
	
	sllx	%o5, %l4, %o5
	or	%g1, %o4, %g6
		
	stx	%g6, [%l1+4*8]
	inc	5*8, %l1
	
	mov	%o5, %o0				! Save our unused data
	dec	5*8, %l2
2:
	inccc	12*8, %l2
	bz,pn	%icc, Lmemcpy_complete
	
	!! Unrolled 8 times
Lmemcpy_aligned8:	
!	ldx	[%l0], %o0				! Already done
!	sllx	%o0, %l4, %o0				! Shift high word
	
	 deccc	8, %l2					! Pre-decrement
	bl,pn	CCCR, Lmemcpy_finish
1:
	ldx	[%l0+8], %o1				! Load word 0
	inc	8, %l0
	
	srlx	%o1, %l3, %g6
	or	%g6, %o0, %g6				! Combine
	
	stx	%g6, [%l1]				! Store result
	 inc	8, %l1
	
	deccc	8, %l2
	bge,pn	CCCR, 1b
	 sllx	%o1, %l4, %o0	

	btst	7, %l2					! Done?
	bz,pt	CCCR, Lmemcpy_complete

	!!
	!! Loadup the last dregs into %o0 and shift it into place
	!! 
	 srlx	%l3, 3, %g6				! # bytes in %o0
	dec	8, %g6					!  - 8
	!! n-8 - (by - 8) -> n - by
	subcc	%l2, %g6, %g0				! # bytes we need
	ble,pt	%icc, Lmemcpy_finish
	 nop
	ldx	[%l0+8], %o1				! Need another word
	srlx	%o1, %l3, %o1
	ba,pt	%icc, Lmemcpy_finish
	 or	%o0, %o1, %o0				! All loaded up.
	
Lmemcpy_noshift8:
	deccc	6*8, %l2				! Have enough room?
	bl,pn	CCCR, 2f
	 nop
	ba,pt	%icc, 1f
	 nop
	.align	32
1:	
	ldx	[%l0+0*8], %o0
	ldx	[%l0+1*8], %o1
	ldx	[%l0+2*8], %o2
	stx	%o0, [%l1+0*8]
	stx	%o1, [%l1+1*8]
	stx	%o2, [%l1+2*8]

	
	ldx	[%l0+3*8], %o3
	ldx	[%l0+4*8], %o4
	ldx	[%l0+5*8], %o5
	inc	6*8, %l0
	stx	%o3, [%l1+3*8]
	deccc	6*8, %l2
	stx	%o4, [%l1+4*8]
	stx	%o5, [%l1+5*8]
	bge,pt	CCCR, 1b
	 inc	6*8, %l1
2:
	inc	6*8, %l2
1:	
	deccc	8, %l2
	bl,pn	%icc, 1f				! < 0 --> sub word
	 nop
	ldx	[%l0], %g6
	inc	8, %l0
	stx	%g6, [%l1]
	bg,pt	%icc, 1b				! Exactly 0 --> done
	 inc	8, %l1
1:
	btst	7, %l2					! Done?
	bz,pt	CCCR, Lmemcpy_complete
	 clr	%l4
	ldx	[%l0], %o0
Lmemcpy_finish:
	
	brz,pn	%l2, 2f					! 100% complete?
	 cmp	%l2, 8					! Exactly 8 bytes?
	bz,a,pn	CCCR, 2f
	 stx	%o0, [%l1]

	btst	4, %l2					! Word store?
	bz	CCCR, 1f
	 srlx	%o0, 32, %g6				! Shift high word down
	stw	%g6, [%l1]
	inc	4, %l1
	mov	%o0, %g6				! Operate on the low bits
1:
	btst	2, %l2
	mov	%g6, %o0
	bz	1f
	 srlx	%o0, 16, %g6
	
	sth	%g6, [%l1]				! Store short
	inc	2, %l1
	mov	%o0, %g6				! Operate on low bytes
1:
	mov	%g6, %o0
	btst	1, %l2					! Byte aligned?
	bz	2f
	 srlx	%o0, 8, %g6

	stb	%g6, [%l1]				! Store last byte
	inc	1, %l1					! Update address
2:	
Lmemcpy_complete:
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
	mov	%i0, %o2
	mov	%i1, %o1
	call	printf
	 mov	%i2, %o3
	ta	1
	.data
0:	.asciz	"memcpy failed: %x@%p != %x@%p byte %d\n"
1:	.asciz	"memcpy(%p, %p, %lx)\n"
	.align 8
	.text
2:	
#endif
	ret
	 restore %i1, %g0, %o0

#ifdef USE_BLOCK_STORE_LOAD

/*
 * Block copy.  Useful for >256 byte copies.
 *
 * Benchmarking has shown this always seems to be slower than
 * the integer version, so this is disabled.  Maybe someone will
 * figure out why sometime.
 */
	
Lmemcpy_block:
	sethi	%hi(block_disable), %o3
	ldx	[ %o3 + %lo(block_disable) ], %o3
	brnz,pn	%o3, Lmemcpy_fancy
	!! Make sure our trap table is installed
	set	_C_LABEL(trapbase), %o5
	rdpr	%tba, %o3
	sub	%o3, %o5, %o3
	brnz,pn	%o3, Lmemcpy_fancy	! No, then don't use block load/store
	 nop
#if defined(_KERNEL) && !defined(_RUMPKERNEL)
/*
 * Kernel:
 *
 * Here we use VIS instructions to do a block clear of a page.
 * But before we can do that we need to save and enable the FPU.
 * The last owner of the FPU registers is fplwp, and
 * fplwp->l_md.md_fpstate is the current fpstate.  If that's not
 * null, call savefpstate() with it to store our current fp state.
 *
 * Next, allocate an aligned fpstate on the stack.  We will properly
 * nest calls on a particular stack so this should not be a problem.
 *
 * Now we grab either curlwp (or if we're on the interrupt stack
 * lwp0).  We stash its existing fpstate in a local register and
 * put our new fpstate in curlwp->p_md.md_fpstate.  We point
 * fplwp at curlwp (or lwp0) and enable the FPU.
 *
 * If we are ever preempted, our FPU state will be saved in our
 * fpstate.  Then, when we're resumed and we take an FPDISABLED
 * trap, the trap handler will be able to fish our FPU state out
 * of curlwp (or lwp0).
 *
 * On exiting this routine we undo the damage: restore the original
 * pointer to curlwp->p_md.md_fpstate, clear our fplwp, and disable
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
 * %l1		fplwp (hi bits only)
 * %l2		orig fplwp
 * %l3		orig fpstate
 * %l5		curlwp
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

	ENABLE_FPU(0)

	mov	%i0, %o0				! Src addr.
	mov	%i1, %o1				! Store our dest ptr here.
	mov	%i2, %o2				! Len counter
#endif	/* _KERNEL */

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
	be,pt	CCCR, 1f				! Already loaded?
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
	be,pt	CCCR, 1f
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
	brz,pn	%o2, Lmemcpy_blockfinish			! XXXX

	 btst	4, %o1
	bz	4f

	mov	-4, %o4
	alignaddr %o0, %o4, %o4				! calculate shift mask and dest.

	cmp	%o3, %o4				! Addresses same?
	beq,pt	CCCR, 1f
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
	brz,pn	%o2, Lmemcpy_blockfinish			! XXXX
	!!
	!! We are now 32-bit aligned in the dest.
	!!
Lmemcpy_block_common:	

	 mov	-0, %o4
	alignaddr %o0, %o4, %o4				! base - shift

	cmp	%o3, %o4				! Addresses same?
	beq,pt	CCCR, 1f
	 mov	%o4, %o3
	fmovd	%f2, %f0				! Shuffle data
	ldd	[%o3+8], %f2				! Load word 0
1:	
	add	%o3, 8, %o0				! now use %o0 for src
	
	!!
	!! Continue until our dest is block aligned
	!! 
Lmemcpy_block_aligned8:	
1:
	brz	%o2, Lmemcpy_blockfinish
	 btst	BLOCK_ALIGN, %o1			! Block aligned?
	bz	1f
	
	 faligndata %f0, %f2, %f4			! Generate result
	deccc	8, %o2
	ble,pn	%icc, Lmemcpy_blockfinish		! Should never happen
	 fmovd	%f4, %f48
	
	std	%f4, [%o1]				! Store result
	inc	8, %o1
	
	fmovd	%f2, %f0
	inc	8, %o0
	ba,pt	%xcc, 1b				! Not yet.
	 ldd	[%o0], %f2				! Load next part
Lmemcpy_block_aligned64:	
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
	add	%o3, (Lmemcpy_block_jmp - 1b), %o3
	jmpl	%o4 + %o3, %g0
	 nop

	!!
	!! Jump table
	!!
	
Lmemcpy_block_jmp:
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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

	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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
	brlez,pn	%o2, Lmemcpy_blockdone
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

	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f10, %f12, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1
	
Lmemcpy_blockdone:
	inc	BLOCK_SIZE, %o2				! Fixup our overcommit
	membar	#Sync					! Finish any pending loads
#define	FINISH_REG(f)				\
	deccc	8, %o2;				\
	bl,a	Lmemcpy_blockfinish;		\
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
Lmemcpy_blockfinish:
	brz,pn	%o2, 2f					! 100% complete?
	 fmovd	%f48, %f4
	cmp	%o2, 8					! Exactly 8 bytes?
	bz,a,pn	CCCR, 2f
	 std	%f4, [%o1]

	btst	4, %o2					! Word store?
	bz	CCCR, 1f
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
	mov	%i0, %o2
	mov	%i1, %o1
	call	prom_printf
	 mov	%i2, %o3
	ta	1
	.data
	_ALIGN
0:	.asciz	"block memcpy failed: %x@%p != %x@%p byte %d\r\n"
1:	.asciz	"memcpy(%p, %p, %lx)\r\n"
	_ALIGN
	.text
2:	
#endif
#if defined(_KERNEL) && !defined(_RUMPKERNEL)

/*
 * Weve saved our possible fpstate, now disable the fpu
 * and continue with life.
 */
	RESTORE_FPU
	ret
	 restore	%g1, 0, %o0			! Return DEST for memcpy
#endif
 	retl
	 mov	%g1, %o0
/*
 * Use block_disable to turn off block insns for
 * memcpy/memset
 */
	.data
	.align	8
	.globl	block_disable
block_disable:	.xword	1
	.text
#endif	/* USE_BLOCK_STORE_LOAD */
