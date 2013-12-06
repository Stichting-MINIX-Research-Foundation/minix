/*	$NetBSD: memset.S,v 1.2 2013/03/17 02:12:41 christos Exp $	*/

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
RCSID("$NetBSD: memset.S,v 1.2 2013/03/17 02:12:41 christos Exp $")
#endif  /* LIBC_SCCS and not lint */


/*
 * XXXXXXXXXXXXXXXXXXXX
 * We need to make sure that this doesn't use floating point
 * before our trap handlers are installed or we could panic
 * XXXXXXXXXXXXXXXXXXXX
 */
/*
 * memset(addr, c, len)
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
#if !defined(_KERNEL) || defined(_RUMPKERNEL)
ENTRY(bzero)
	! %o0 = addr, %o1 = len
	mov	%o1, %o2
	clr	%o1			! ser pattern
#endif	
ENTRY(memset)
	! %o0 = addr, %o1 = pattern, %o2 = len
	mov	%o0, %o4		! Save original pointer

Lmemset_internal:
	btst	7, %o0			! Word aligned?
	bz,pn	%xcc, 0f
	 nop
	inc	%o0
	deccc	%o2			! Store up to 7 bytes
	bge,a,pt	CCCR, Lmemset_internal
	 stb	%o1, [%o0 - 1]

	retl				! Duplicate Lmemset_done
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
#ifdef USE_BLOCK_STORE_LOAD
	!! Now we are 64-bit aligned
	cmp	%o2, 256		! Use block clear if len > 256
	bge,pt	CCCR, Lmemset_block	! use block store insns
#endif	/* USE_BLOCK_STORE_LOAD */
	 deccc	8, %o2
Lmemset_longs:
	bl,pn	CCCR, Lmemset_cleanup	! Less than 8 bytes left
	 nop
3:	
	inc	8, %o0
	deccc	8, %o2
	bge,pt	CCCR, 3b
	 stx	%o1, [%o0 - 8]		! Do 1 longword at a time

	/*
	 * Len is in [-8..-1] where -8 => done, -7 => 1 byte to zero,
	 * -6 => two bytes, etc.  Mop up this remainder, if any.
	 */
Lmemset_cleanup:	
	btst	4, %o2
	bz,pt	CCCR, 5f		! if (len & 4) {
	 nop
	stw	%o1, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
5:	
	btst	2, %o2
	bz,pt	CCCR, 7f		! if (len & 2) {
	 nop
	sth	%o1, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
7:	
	btst	1, %o2
	bnz,a	%icc, Lmemset_done	! if (len & 1)
	 stb	%o1, [%o0]		!	*addr = 0;
Lmemset_done:
	retl
	 mov	%o4, %o0		! Restore ponter for memset (ugh)

#ifdef USE_BLOCK_STORE_LOAD
Lmemset_block:
	sethi	%hi(block_disable), %o3
	ldx	[ %o3 + %lo(block_disable) ], %o3
	brnz,pn	%o3, Lmemset_longs
	!! Make sure our trap table is installed
	set	_C_LABEL(trapbase), %o5
	rdpr	%tba, %o3
	sub	%o3, %o5, %o3
	brnz,pn	%o3, Lmemset_longs	! No, then don't use block load/store
	 nop
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
 */

	ENABLE_FPU(0)

	!! We are now 8-byte aligned.  We need to become 64-byte aligned.
	btst	63, %i0
	bz,pt	CCCR, 2f
	 nop
1:
	stx	%i1, [%i0]
	inc	8, %i0
	btst	63, %i0
	bnz,pt	%xcc, 1b
	 dec	8, %i2

2:
	brz	%i1, 3f					! Skip the memory op
	 fzero	%f0					! if pattern is 0

#ifdef _LP64
	stx	%i1, [%i0]				! Flush this puppy to RAM
	membar	#StoreLoad
	ldd	[%i0], %f0
#else
	stw	%i1, [%i0]				! Flush this puppy to RAM
	membar	#StoreLoad
	ld	[%i0], %f0
	fmovsa	%icc, %f0, %f1
#endif
	
3:	
	fmovd	%f0, %f2				! Duplicate the pattern
	fmovd	%f0, %f4
	fmovd	%f0, %f6
	fmovd	%f0, %f8
	fmovd	%f0, %f10
	fmovd	%f0, %f12
	fmovd	%f0, %f14

	!! Remember: we were 8 bytes too far
	dec	56, %i2					! Go one iteration too far
5:
	stda	%f0, [%i0] ASI_STORE			! Store 64 bytes
	deccc	BLOCK_SIZE, %i2
	bg,pt	%icc, 5b
	 inc	BLOCK_SIZE, %i0

	membar	#Sync
/*
 * We've saved our possible fpstate, now disable the fpu
 * and continue with life.
 */
	RESTORE_FPU
	addcc	%i2, 56, %i2				! Restore the count
	ba,pt	%xcc, Lmemset_longs			! Finish up the remainder
	 restore
#endif	/* USE_BLOCK_STORE_LOAD */
