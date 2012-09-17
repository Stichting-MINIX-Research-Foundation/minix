/*	$NetBSD: bzero.S,v 1.11 2011/01/29 02:21:20 matt Exp $ */

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


#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: bzero.S,v 1.11 2011/01/29 02:21:20 matt Exp $")
#endif /* LIBC_SCCS && !lint */

#ifdef _KERNEL
#include <assym.h>
#endif

#define USE_STSWX 0	/* don't. slower than trivial copy loop */

/*----------------------------------------------------------------------*/
/*
     void bzero(void *b %r3, size_t len %r4);
     void * memset(void *b %r3, int c %r4, size_t len %r5);
*/
/*----------------------------------------------------------------------*/

#define r_dst	%r3
#define r_len	%r4
#define r_val	%r0

		.text
		.align 4
ENTRY(bzero)
		li	r_val, 0		/* Value to stuff in */
		b	cb_memset
END(bzero)

ENTRY(memset)
		cmplwi	%cr1, %r5, 0
		mr.	%r0, %r4
		mr	%r8, %r3
		beqlr-	%cr1			/* Nothing to do */

		rlwimi	%r0, %r4, 8, 16, 23	/* word extend fill value */
		rlwimi	%r0, %r0, 16, 0, 15
		mr	%r4, %r5
		bne-	simple_fill		/* =! 0, use trivial fill */
cb_memset:

/*----------------------------------------------------------------------*/
#ifndef _KERNEL
		/* First find out cache line size */
		mflr	%r9
#ifdef PIC
		PIC_GOTSETUP(%r10)
		mtlr	%r9
		lwz	%r5,cache_info@got(%r10)
#else
		lis	%r5,cache_info@h
		ori	%r5,%r5,cache_info@l
#endif
		lwz	%r6, 4(%r5)
		cmpwi	%r6, -1
		bne+	cb_cacheline_known

/*----------------------------------------------------------------------*/
#define CTL_MACHDEP	7
#define CPU_CACHELINE	1
#define	CPU_CACHEINFO	5

#define STKFRAME_SZ	64
#define MIB		8
#define OLDPLEN		16
#define R3_SAVE		20
#define R4_SAVE		24
#define R0_SAVE		28
#define R8_SAVE		32
#define R31_SAVE	36
#ifdef PIC
#define R30_SAVE	40
#endif

		stw	%r9, 4(%r1)
		stwu	%r1, -STKFRAME_SZ(%r1)

		stw	%r31, R31_SAVE(%r1)
		mr	%r31, %r5		/* cache info */

#ifdef PIC
		stw	%r30, R30_SAVE(%r1)
		PIC_TOCSETUP(cb_memset,%r30)
#endif

		stw	%r8, R8_SAVE(%r1)
		stw	%r3, R3_SAVE(%r1)
		stw	%r4, R4_SAVE(%r1)
		stw	%r0, R0_SAVE(%r1)

		li	%r0, CTL_MACHDEP		/* Construct MIB */
		stw	%r0, MIB(%r1)
		li	%r0, CPU_CACHEINFO
		stw	%r0, MIB+4(%r1)

		li	%r0, 4*4			/* Oldlenp := 4*4 */
		stw	%r0, OLDPLEN(%r1)

		addi	%r3, %r1, MIB
		li	%r4, 2			/* namelen */
		/* %r5 already contains &cache_info */
		addi	%r6, %r1, OLDPLEN
		li	%r7, 0
		li	%r8, 0
		bl	PIC_PLT(_C_LABEL(sysctl))
	
		cmpwi	%r3, 0			/* Check result */
		beq	1f

		/* Failure, try older sysctl */
	
		li	%r0, CTL_MACHDEP	/* Construct MIB */
		stw	%r0, MIB(%r1)
		li	%r0, CPU_CACHELINE
		stw	%r0, MIB+4(%r1)

		li	%r0, 4			/* Oldlenp := 4 */
		stw	%r0, OLDPLEN(%r1)

		addi	%r3, %r1, MIB
		li	%r4, 2			/* namelen */
		addi	%r5, %r31, 4
		addi	%r6, %r1, OLDPLEN
		li	%r7, 0
		li	%r8, 0
		bl	PIC_PLT(_C_LABEL(sysctl))
1:
		lwz	%r3, R3_SAVE(%r1)
		lwz	%r4, R4_SAVE(%r1)
		lwz	%r8, R8_SAVE(%r1)
		lwz	%r0, R0_SAVE(%r1)
		lwz	%r9, 4(%r31)
		lwz	%r31, R31_SAVE(%r1)
#ifdef PIC
		lwz	%r30, R30_SAVE(%r1)
#endif
		addi	%r1, %r1, STKFRAME_SZ
		lwz	%r7, 4(%r1)
		mtlr	%r7

		cntlzw	%r6, %r9			/* compute shift value */
		li	%r5, 31
		subf	%r5, %r6, %r5

#ifdef PIC
		mflr	%r9
		PIC_GOTSETUP(%r10)
		mtlr	%r9
		lwz	%r6, cache_sh@got(%r10)
		stw	%r5, 0(%r6)
#else
		lis	%r6, cache_sh@ha
		stw	%r5, cache_sh@l(%r6)
#endif
/*----------------------------------------------------------------------*/
/* Okay, we know the cache line size (%r9) and shift value (%r10) */
cb_cacheline_known:
#ifdef PIC
		lwz	%r5, cache_info@got(%r10)
		lwz	%r9, 4(%r5)
		lwz	%r5, cache_sh@got(%r10)
		lwz	%r10, 0(%r5)
#else
		lis	%r9, cache_info+4@ha
		lwz	%r9, cache_info+4@l(%r9)
		lis	%r10, cache_sh@ha
		lwz	%r10, cache_sh@l(%r10)
#endif

#else /* _KERNEL */
#ifdef	MULTIPROCESSOR
		mfsprg	%r10, 0			/* Get cpu_info pointer */
#else
		lis	%r10, cpu_info_store@ha
		addi	%r10, %r10, cpu_info_store@l
#endif
		lwz	%r9, CPU_CI+4(%r10)	/* Load D$ line size */
		cntlzw	%r10, %r9			/* Calculate shift.. */
		li	%r6, 31
		subf	%r10, %r10, %r6
#endif /* _KERNEL */
		/* Back in memory filling business */
		
		cmplwi	%cr1, r_len, 0		/* Nothing to do? */
		add	%r5, %r9, %r9
		cmplw	r_len, %r5		/* <= 2*CL bytes to move? */
		beqlr-	%cr1			/* then do nothing */

		blt+	simple_fill		/* a trivial fill routine */

		/* Word align the block, fill bytewise until dst even*/
		
		andi.	%r5, r_dst, 0x03	
		li	%r6, 4
		beq+	cb_aligned_w		/* already aligned to word? */

		subf	%r5, %r5, %r6		/* bytes to fill to align4 */
#if USE_STSWX
		mtxer	%r5
		stswx	%r0, 0, r_dst
		add	r_dst, %r5, r_dst
#else
		mtctr	%r5

		subi	r_dst, r_dst, 1
1:		stbu	r_val, 1(r_dst)		/* Fill bytewise */
		bdnz	1b

		addi	r_dst, r_dst, 1
#endif
		subf	r_len, %r5, r_len

cb_aligned_w:	/* Cache block align, fill wordwise until dst aligned */

		/* I know I have something to do since we had > 2*CL initially */
		/* so no need to check for r_len = 0 */

		subi	%r6, %r9, 1		/* CL mask */
		and.	%r5, r_dst, %r6
		srwi	%r5, %r5, 2
		srwi	%r6, %r9, 2
		beq	cb_aligned_cb		/* already on CL boundary? */

		subf	%r5, %r5, %r6		/* words to fill to alignment */
		mtctr	%r5
		slwi	%r5, %r5, 2
		subf	r_len, %r5, r_len

		subi	r_dst, r_dst, 4
1:		stwu	r_val, 4(r_dst)		/* Fill wordwise */
		bdnz	1b
		addi	r_dst, r_dst, 4

cb_aligned_cb:	/* no need to check r_len, see above */
		
		srw.	%r5, r_len, %r10		/* Number of cache blocks */
		mtctr	%r5
		beq	cblocks_done

		slw	%r5, %r5, %r10
		subf	r_len, %r5, r_len

1:		dcbz	0, r_dst		/* Clear blockwise */
		add	r_dst, r_dst, %r9
		bdnz	1b

cblocks_done:	/* still CL aligned, but less than CL bytes left */
		cmplwi	%cr1, r_len, 0
		cmplwi	r_len, 8
		beq-	%cr1, sf_return

		blt-	sf_bytewise		/* <8 remaining? */
		b	sf_aligned_w

/*----------------------------------------------------------------------*/
wbzero:		li	r_val, 0

		cmplwi	r_len, 0
		beqlr-				/* Nothing to do */

simple_fill:
#if USE_STSWX
		cmplwi	%cr1, r_len, 12		/* < 12 bytes to move? */
#else
		cmplwi	%cr1, r_len, 8		/* < 8 bytes to move? */
#endif
		andi.	%r5, r_dst, 0x03		/* bytes to fill to align4 */
		blt	%cr1, sf_bytewise	/* trivial byte mover */

		li	%r6, 4
		subf	%r5, %r5, %r6
		beq+	sf_aligned_w		/* dest is word aligned */

#if USE_STSWX
		mtxer	%r5
		stswx	%r0, 0, r_dst
		add	r_dst, %r5, r_dst
#else
		mtctr	%r5			/* nope, then fill bytewise */
		subi	r_dst, r_dst, 1		/* until it is */
1:		stbu	r_val, 1(r_dst)		
		bdnz	1b

		addi	r_dst, r_dst, 1
#endif
		subf	r_len, %r5, r_len

sf_aligned_w:	/* no need to check r_len since it were >= 8 bytes initially */
#if USE_STSWX
		mr	%r6, %r0
		mr	%r7, %r0

		srwi	%r5, r_len, 3
		mtctr	%r5
		
		slwi	%r5, %r5, 3		/* adjust len */
		subf.	r_len, %r5, r_len
		
1:		stswi	%r6, r_dst, 8
		addi	r_dst, r_dst, 8
		bdnz	1b
#else
		srwi	%r5, r_len, 2		/* words to fill */
		mtctr	%r5

		slwi	%r5, %r5, 2
		subf.	r_len, %r5, r_len	/* adjust len for fill */

		subi	r_dst, r_dst, 4
1:		stwu	r_val, 4(r_dst)
		bdnz	1b
		addi	r_dst, r_dst, 4
#endif

sf_word_done:	bne-	sf_bytewise

sf_return:	mr	%r3, %r8			/* restore orig ptr */
		blr				/* for memset functionality */

sf_bytewise:
#if USE_STSWX
		mr	%r5, %r0
		mr	%r6, %r0
		mr	%r7, %r0
		
		mtxer	r_len
		stswx	%r5, 0, r_dst
#else
		mtctr	r_len

		subi	r_dst, r_dst, 1
1:		stbu	r_val, 1(r_dst)
		bdnz	1b
#endif
		mr	%r3, %r8			/* restore orig ptr */
		blr				/* for memset functionality */
END(memset)

/*----------------------------------------------------------------------*/
#ifndef _KERNEL
		.data
cache_info:	.long	-1, -1, -1, -1
cache_sh:	.long	0

#endif
/*----------------------------------------------------------------------*/
