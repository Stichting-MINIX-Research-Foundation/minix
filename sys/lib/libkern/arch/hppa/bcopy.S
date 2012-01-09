/*	$NetBSD: bcopy.S,v 1.14 2011/01/31 12:10:58 skrll Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthew Fredette.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copy routines for NetBSD/hppa.
 */

#undef _LOCORE
#define _LOCORE	/* XXX fredette - unfortunate */

#if defined(SPCOPY) && !defined(_STANDALONE)

#include "opt_multiprocessor.h"

#include <machine/cpu.h>

#endif

#include <machine/asm.h>
#include <machine/frame.h>
#include <machine/reg.h>

#if defined(LIBC_SCCS) && !defined(lint)
RCSID("$NetBSD: bcopy.S,v 1.14 2011/01/31 12:10:58 skrll Exp $")
#endif /* LIBC_SCCS and not lint */

/*
 * The stbys instruction is a little asymmetric.  When (%r2 & 3)
 * is zero, stbys,b,m %r1, 4(%r2) works like stws,ma.  You
 * might then wish that when (%r2 & 3) == 0, stbys,e,m %r1, -4(%r2)
 * worked like stws,mb.  But it doesn't.
 *
 * This macro works around this problem.  It requires that %t2
 * hold the number of bytes that will be written by this store
 * (meaning that it ranges from one to four).
 *
 * Watch the delay-slot trickery here.  The comib is used to set
 * up which instruction, either the stws or the stbys, is run
 * in the delay slot of the b instruction.
 */
#define _STBYS_E_M(r, dst_spc, dst_off)				  \
	comib,<>	4, %t2, 4				! \
	b		4					! \
	stws,mb		r, -4(dst_spc, dst_off)			! \
	stbys,e,m	r, 0(dst_spc, dst_off)

/*
 * This macro does a bulk copy with no shifting.  cmplt and m are
 * the completer and displacement multiplier, respectively, for
 * the load and store instructions.
 */
#define _COPY(src_spc, src_off, dst_spc, dst_off, count, cmplt, m) \
								! \
	/*							! \
	 * Loop storing 16 bytes at a time.  Since count 	! \
	 * may be > INT_MAX, we have to be careful and		! \
	 * avoid comparisons that treat it as a signed 		! \
	 * quantity, until after this loop, when count		! \
	 * is guaranteed to be less than 16.			! \
	 */							! \
	comib,>>=,n	15, count, _LABEL(_skip16)		! \
.label _LABEL(_loop16)						! \
	addi		-16, count, count			! \
	ldws,cmplt	m*4(src_spc, src_off), %t1		! \
	ldws,cmplt	m*4(src_spc, src_off), %t2		! \
	ldws,cmplt	m*4(src_spc, src_off), %t3		! \
	ldws,cmplt	m*4(src_spc, src_off), %t4		! \
	stws,cmplt	%t1, m*4(dst_spc, dst_off)		! \
	stws,cmplt	%t2, m*4(dst_spc, dst_off)		! \
	stws,cmplt	%t3, m*4(dst_spc, dst_off)		! \
	comib,<<	15, count, _LABEL(_loop16)		! \
	stws,cmplt	%t4, m*4(dst_spc, dst_off)		! \
.label _LABEL(_skip16)						! \
								! \
	/* Loop storing 4 bytes at a time. */			! \
	addib,<,n	-4, count, _LABEL(_skip4)		! \
.label _LABEL(_loop4)						! \
	ldws,cmplt	m*4(src_spc, src_off), %t1		! \
	addib,>=	-4, count, _LABEL(_loop4)		! \
	stws,cmplt	%t1, m*4(dst_spc, dst_off)		! \
.label _LABEL(_skip4)						! \
	/* Restore the correct count. */			! \
	addi		4, count, count				! \
								! \
.label _LABEL(_do1)						! \
								! \
	/* Loop storing 1 byte at a time. */			! \
	addib,<,n	-1, count, _LABEL(_skip1)		! \
.label _LABEL(_loop1)						! \
	ldbs,cmplt	m*1(src_spc, src_off), %t1		! \
	addib,>=	-1, count, _LABEL(_loop1)		! \
	stbs,cmplt	%t1, m*1(dst_spc, dst_off)		! \
.label _LABEL(_skip1)						! \
	/* Restore the correct count. */			! \
	b		_LABEL(_done)				! \
	addi		1, count, count

/*
 * This macro is definitely strange.  It exists purely to
 * allow the _COPYS macro to be reused, but because it
 * requires this long attempt to explain it, I'm starting
 * to doubt the value of that.
 *
 * Part of the expansion of the _COPYS macro below are loops
 * that copy four words or one word at a time, performing shifts
 * to get data to line up correctly in the destination buffer.
 *
 * The _COPYS macro is used when copying backwards, as well
 * as forwards.  The 4-word loop always loads into %t1, %t2, %t3,
 * and %t4 in that order.  This means that when copying forward,
 * %t1 will have the word from the lowest address, and %t4 will
 * have the word from the highest address.  When copying
 * backwards, the opposite is true.
 *
 * The shift instructions need pairs of registers with adjacent
 * words, with the register containing the word from the lowest
 * address *always* coming first.  It is this assymetry that
 * gives rise to this macro - depending on which direction
 * we're copying in, these ordered pairs are different.
 *
 * Fortunately, we can compute those register numbers at compile
 * time, and assemble them manually into a shift instruction.
 * That's what this macro does.
 *
 * This macro takes two arguments.  n ranges from 0 to 3 and
 * is the "shift number", i.e., n = 0 means we're doing the
 * shift for what will be the first store.
 *
 * m is the displacement multiplier from the _COPYS macro call.
 * This is 1 for a forward copy and -1 for a backwards copy.
 * So, the ((m + 1) / 2) term yields 0 for a backwards copy and
 * 1 for a forward copy, and the ((m - 1) / 2) term yields
 * 0 for a forward copy, and -1 for a backwards copy.
 * These terms are used to discriminate the register computations
 * below.
 *
 * When copying forward, then, the first register used with
 * the first vshd will be 19 + (3 - ((0 - 1) & 3)), or %t4,
 * which matches _COPYS' requirement that the word last loaded
 * be in %t4.  The first register used for the second vshd
 * will then "wrap" around to 19 + (3 - ((1 - 1) & 3)), or %t1.
 * And so on to %t2 and %t3.
 *
 * When copying forward, the second register used with the first
 * vshd will be (19 + (3 - ((n + 0) & 3)), or %t1.  It will
 * continue to be %t2, then %t3, and finally %t4.
 *
 * When copying backwards, the values for the first and second
 * register for each vshd are reversed from the forwards case.
 * (Symmetry reclaimed!)  Proving this is "left as an exercise
 * for the reader" (remember the different discriminating values!)
 */
#define _VSHD(n, m, t)						  \
	.word (0xd0000000					| \
	((19 + (3 - ((n - 1 * ((m + 1) / 2)) & 3))) << 16)	| \
	((19 + (3 - ((n + 1 * ((m - 1) / 2)) & 3))) << 21)	| \
	(t))

/*
 * This macro does a bulk copy with shifting.  cmplt and m are
 * the completer and displacement multiplier, respectively, for
 * the load and store instructions.  It is assumed that the
 * word last loaded is already in %t4.
 */
#define _COPYS(src_spc, src_off, dst_spc, dst_off, count, cmplt, m) \
								! \
	/*							! \
	 * Loop storing 16 bytes at a time.  Since count 	! \
	 * may be > INT_MAX, we have to be careful and		! \
	 * avoid comparisons that treat it as a signed 		! \
	 * quantity, until after this loop, when count		! \
	 * is guaranteed to be less than 16.			! \
	 */							! \
	comib,>>=,n	15, count, _LABEL(S_skip16)		! \
.label _LABEL(S_loop16)						! \
	addi		-16, count, count			! \
	ldws,cmplt	m*4(src_spc, src_off), %t1		! \
	ldws,cmplt	m*4(src_spc, src_off), %t2		! \
	ldws,cmplt	m*4(src_spc, src_off), %t3		! \
	_VSHD(0, m, 1)	/* vshd %t4, %t1, %r1 */		! \
	ldws,cmplt	m*4(src_spc, src_off), %t4		! \
	_VSHD(1, m, 22)	/* vshd %t1, %t2, %t1 */		! \
	_VSHD(2, m, 21)	/* vshd %t2, %t3, %t2 */		! \
	_VSHD(3, m, 20)	/* vshd %t3, %t4, %t3 */		! \
	stws,cmplt	%r1, m*4(dst_spc, dst_off)		! \
	stws,cmplt	%t1, m*4(dst_spc, dst_off)		! \
	stws,cmplt	%t2, m*4(dst_spc, dst_off)		! \
	comib,<<	15, count, _LABEL(S_loop16)		! \
	stws,cmplt	%t3, m*4(dst_spc, dst_off)		! \
.label _LABEL(S_skip16)						! \
								! \
	/* Loop storing 4 bytes at a time. */			! \
	addib,<,n	-4, count, _LABEL(S_skip4)		! \
.label _LABEL(S_loop4)						! \
	ldws,cmplt	m*4(src_spc, src_off), %t1		! \
	_VSHD(0, m, 1)	/* into %r1 (1) */			! \
	copy		%t1, %t4				! \
	addib,>=	-4, count, _LABEL(S_loop4)		! \
	stws,cmplt	%r1, m*4(dst_spc, dst_off)		! \
.label _LABEL(S_skip4)						! \
								! \
	/*							! \
 	 * We now need to "back up" src_off by the		! \
	 * number of bytes remaining in the FIFO		! \
	 * (i.e., the number of bytes remaining in %t4),	! \
	 * because (the correct) count still includes		! \
	 * these bytes, and we intent to keep it that		! \
	 * way, and finish with the single-byte copier.		! \
	 *							! \
	 * The number of bytes remaining in the FIFO is		! \
	 * related to the shift count, so recover it,		! \
	 * restoring the correct count at the same time.	! \
	 */							! \
	mfctl	%cr11, %t1					! \
	addi	4, count, count					! \
	shd	%r0, %t1, 3, %t1				! \
								! \
	/*							! \
	 * If we're copying forward, the shift count		! \
	 * is the number of bytes remaining in the		! \
	 * FIFO, and we want to subtract it from src_off.	! \
	 * If we're copying backwards, (4 - shift count)	! \
	 * is the number of bytes remaining in the FIFO,	! \
	 * and we want to add it to src_off.			! \
	 *							! \
	 * We observe that x + (4 - y) = x - (y - 4),		! \
	 * and introduce this instruction to add -4 when	! \
	 * m is -1, although this does mean one extra		! \
	 * instruction in the forward case.			! \
	 */							! \
	addi	4*((m - 1) / 2), %t1, %t1			! \
								! \
	/* Now branch to the byte-at-a-time loop. */		! \
	b	_LABEL(_do1)					! \
	sub	src_off, %t1, src_off

/*
 * This macro copies a region in the forward direction.
 */
#define _COPY_FORWARD(src_spc, src_off, dst_spc, dst_off, count)  \
								! \
	/*							! \
	 * Since in the shifting-left case we will		! \
	 * load 8 bytes before checking count, to		! \
	 * keep things simple, branch to the byte 		! \
	 * copier unless we're copying at least 8.		! \
	 */							! \
	comib,>>,n	8, count, _LABEL(_do1)			! \
								! \
	/*							! \
	 * Once we 4-byte align the source offset, 		! \
	 * figure out how many bytes from the region		! \
	 * will be in the first 4-byte word we read.		! \
	 * Ditto for writing the destination offset.		! \
	 */							! \
	extru		src_off, 31, 2, %t1			! \
	extru		dst_off, 31, 2, %t2			! \
	subi		4, %t1, %t1				! \
	subi		4, %t2, %t2				! \
								! \
	/*							! \
	 * Calculate the byte shift required.  A 		! \
	 * positive value means a source 4-byte word 		! \
	 * has to be shifted to the right to line up 		! \
	 * as a destination 4-byte word.			! \
	 */							! \
	sub		%t1, %t2, %t1				! \
								! \
	/* 4-byte align src_off. */				! \
	depi		0, 31, 2, src_off			! \
								! \
	/*							! \
	 * It's somewhat important to note that this		! \
	 * code thinks of count as "the number of bytes		! \
	 * that haven't been stored yet", as opposed to		! \
	 * "the number of bytes that haven't been copied	! \
	 * yet".  The distinction is subtle, but becomes	! \
	 * apparent at the end of the shifting code, where	! \
	 * we "back up" src_off to correspond to count,		! \
	 * as opposed to flushing the FIFO.			! \
	 *							! \
	 * We calculated above how many bytes our first		! \
	 * store will store, so update count now.		! \
	 *							! \
	 * If the shift is zero, strictly as an optimization	! \
	 * we use a copy loop that does no shifting.		! \
	 */							! \
	comb,<>		%r0, %t1, _LABEL(_shifting)		! \
	sub		count, %t2, count			! \
								! \
	/* Load and store the first word. */			! \
	ldws,ma		4(src_spc, src_off), %t4		! \
	stbys,b,m	%t4, 4(dst_spc, dst_off)		! \
								! \
	/* Do the rest of the copy. */				! \
	_COPY(src_spc,src_off,dst_spc,dst_off,count,ma,1)	! \
								! \
.label _LABEL(_shifting)					! \
								! \
	/*							! \
	 * If shift < 0, we need to shift words to the		! \
	 * left.  Since we can't do this directly, we		! \
	 * adjust the shift so it's a shift to the right	! \
	 * and load the first word into the high word of	! \
	 * the FIFO.  Otherwise, we load a zero into the	! \
	 * high word of the FIFO.				! \
	 */							! \
	comb,<=		%r0, %t1, _LABEL(_shiftingrt)		! \
	copy		%r0, %t3				! \
	addi		4, %t1, %t1				! \
	ldws,ma		4(src_spc, src_off), %t3		! \
.label _LABEL(_shiftingrt)					! \
								! \
	/*							! \
	 * Turn the shift byte count into a bit count,		! \
	 * load the next word, set the Shift Amount 		! \
	 * Register, and form and store the first word.		! \
	 */							! \
	sh3add		%t1, %r0, %t1				! \
	ldws,ma		4(src_spc, src_off), %t4		! \
	mtctl		%t1, %cr11				! \
	vshd		%t3, %t4, %r1				! \
	stbys,b,m	%r1, 4(dst_spc, dst_off)		! \
								! \
	/* Do the rest of the copy. */				! \
	_COPYS(src_spc,src_off,dst_spc,dst_off,count,ma,1)

/* This macro copies a region in the reverse direction. */
#define _COPY_REVERSE(src_spc, src_off, dst_spc, dst_off, count)  \
								! \
	/* Immediately add count to both offsets. */		! \
	add	src_off, count, src_off				! \
	add	dst_off, count, dst_off				! \
								! \
	/*							! \
	 * Since in the shifting-right case we 			! \
	 * will load 8 bytes before checking 			! \
	 * count, to keep things simple, branch 		! \
	 * to the byte copier unless we're 			! \
	 * copying at least 8 bytes.				! \
	 */							! \
	comib,>>,n	8, count, _LABEL(_do1)			! \
								! \
	/*							! \
	 * Once we 4-byte align the source offset, 		! \
	 * figure out how many bytes from the region		! \
	 * will be in the first 4-byte word we read.		! \
	 * Ditto for writing the destination offset.		! \
	 */							! \
	extru,<>	src_off, 31, 2, %t1			! \
	ldi		4, %t1					! \
	extru,<>	dst_off, 31, 2, %t2			! \
	ldi		4, %t2					! \
								! \
	/*							! \
	 * Calculate the byte shift required.  A 		! \
	 * positive value means a source 4-byte 		! \
	 * word has to be shifted to the right to 		! \
	 * line up as a destination 4-byte word.		! \
	 */							! \
	sub		%t2, %t1, %t1				! \
								! \
	/*							! \
	 * 4-byte align src_off, leaving it pointing 		! \
	 * to the 4-byte word *after* the next word 		! \
	 * we intend to load.					! \
	 *							! \
	 * It's somewhat important to note that this		! \
	 * code thinks of count as "the number of bytes		! \
	 * that haven't been stored yet", as opposed to		! \
	 * "the number of bytes that haven't been copied	! \
	 * yet".  The distinction is subtle, but becomes	! \
	 * apparent at the end of the shifting code, where	! \
	 * we "back up" src_off to correspond to count,		! \
	 * as opposed to flushing the FIFO.			! \
	 *							! \
	 * We calculated above how many bytes our first		! \
	 * store will store, so update count now.		! \
	 *							! \
	 * If the shift is zero, we use a copy loop that	! \
	 * does no shifting.  NB: unlike the forward case,	! \
	 * this is NOT strictly an optimization.  If the	! \
	 * SAR is zero the vshds do NOT do the right thing.	! \
	 * This is another assymetry more or less the "fault"	! \
	 * of vshd.						! \
	 */							! \
	addi		3, src_off, src_off			! \
	sub		count, %t2, count			! \
	comb,<>		%r0, %t1, _LABEL(_shifting)		! \
	depi		0, 31, 2, src_off			! \
								! \
	/* Load and store the first word. */			! \
	ldws,mb		-4(src_spc, src_off), %t4		! \
	_STBYS_E_M(%t4, dst_spc, dst_off)			! \
								! \
	/* Do the rest of the copy. */				! \
	_COPY(src_spc,src_off,dst_spc,dst_off,count,mb,-1)	! \
								! \
.label _LABEL(_shifting)					! \
								! \
	/*							! \
	 * If shift < 0, we need to shift words to the		! \
	 * left.  Since we can't do this directly, we		! \
	 * adjust the shift so it's a shift to the right	! \
	 * and load a zero in to the low word of the FIFO.	! \
	 * Otherwise, we load the first word into the		! \
	 * low word of the FIFO.				! \
	 *							! \
	 * Note the nullification trickery here.  We 		! \
	 * assume that we're shifting to the left, and		! \
	 * load zero into the low word of the FIFO.  Then	! \
	 * we nullify the addi if we're shifting to the		! \
	 * right.  If the addi is not nullified, we are		! \
 	 * shifting to the left, so we nullify the load.	! \
	 * we branch if we're shifting to the 			! \
	 */							! \
	copy		%r0, %t3				! \
	comb,<=,n	%r0, %t1, 0				! \
	addi,tr		4, %t1, %t1				! \
	ldws,mb		-4(src_spc, src_off), %t3		! \
								! \
	/*							! \
	 * Turn the shift byte count into a bit count,		! \
	 * load the next word, set the Shift Amount 		! \
	 * Register, and form and store the first word.		! \
	 */							! \
	sh3add		%t1, %r0, %t1				! \
	ldws,mb		-4(src_spc, src_off), %t4		! \
	mtctl		%t1, %cr11				! \
	vshd		%t4, %t3, %r1				! \
	_STBYS_E_M(%r1, dst_spc, dst_off)			! \
								! \
	/* Do the rest of the copy. */				! \
	_COPYS(src_spc,src_off,dst_spc,dst_off,count,mb,-1)

/*
 * For paranoia, when things aren't going well, enable this
 * code to assemble byte-at-a-time-only copying.
 */
#if 1
#undef _COPY_FORWARD
#define _COPY_FORWARD(src_spc, src_off, dst_spc, dst_off, count)  \
	comb,=,n	%r0, count, _LABEL(_done)		! \
	ldbs,ma		1(src_spc, src_off), %r1		! \
	addib,<>	-1, count, -12				! \
	stbs,ma		%r1, 1(dst_spc, dst_off)		! \
	b,n		_LABEL(_done)
#undef _COPY_REVERSE
#define _COPY_REVERSE(src_spc, src_off, dst_spc, dst_off, count)  \
	comb,=		%r0, count, _LABEL(_done)		! \
	add		src_off, count, src_off			! \
	add		dst_off, count, dst_off			! \
	ldbs,mb		-1(src_spc, src_off), %r1		! \
	addib,<>	-1, count, -12				! \
	stbs,mb		%r1, -1(dst_spc, dst_off)		! \
	b,n		_LABEL(_done)
#endif

/*
 * If none of the following are defined, define BCOPY.
 */
#if !(defined(SPCOPY) || defined(MEMCPY) || defined(MEMMOVE))
#define BCOPY
#endif

#if defined(SPCOPY) && !defined(_STANDALONE)

#include <sys/errno.h>
#include "assym.h"

/*
 * int spcopy(pa_space_t ssp, const void *src, pa_space_t dsp, void *dst,
 * 	size_t len)
 *
 * We assume that the regions do not overlap.
 */
LEAF_ENTRY(spcopy)

        /*
	 * Setup the fault handler, which will fill in %ret0 if triggered.
	 */
	GET_CURLWP(%r31)
#ifdef	DIAGNOSTIC
	comb,<>,n %r0, %r31, Lspcopy_curlwp_ok
	ldil	L%panic, %r1
	ldil	L%Lspcopy_curlwp_bad, %arg0
	ldo	R%panic(%r1), %r1
	ldo	R%Lspcopy_curlwp_bad(%arg0), %arg0
	.call
	bv,n    %r0(%r1)
	nop
Lspcopy_curlwp_bad:
	.asciz	"spcopy: curlwp == NULL\n"
	.align	8
Lspcopy_curlwp_ok:
#endif /* DIAGNOSTIC */
	ldil    L%spcopy_fault, %r1
	ldw     L_PCB(%r31), %r31
	ldo     R%spcopy_fault(%r1), %r1
	stw     %r1, PCB_ONFAULT(%r31)

	/* Setup the space registers. */
	mfsp	%sr2, %ret1
	mtsp	%arg0, %sr1
	mtsp	%arg2, %sr2

	/* Get the len argument and do the copy. */
	ldw	HPPA_FRAME_ARG(4)(%sp), %arg0
#define	_LABEL(l) __CONCAT(spcopy,l)
	_COPY_FORWARD(%sr1,%arg1,%sr2,%arg3,%arg0)
_LABEL(_done):

	/* Return. */
	copy	%r0, %ret0
ALTENTRY(spcopy_fault)
	stw     %r0, PCB_ONFAULT(%r31)
	bv	%r0(%rp)
	mtsp	%ret1, %sr2
EXIT(spcopy)
#endif /* SPCOPY && !_STANDALONE */

#ifdef MEMCPY
/*
 * void *memcpy(void *restrict dst, const void *restrict src, size_t len);
 *
 * memcpy is specifically restricted to working on
 * non-overlapping regions, so we can just copy forward.
 */
LEAF_ENTRY(memcpy)
	copy	%arg0, %ret0
#define	_LABEL(l) __CONCAT(memcpy,l)
	_COPY_FORWARD(%sr0,%arg1,%sr0,%arg0,%arg2)
_LABEL(_done):
	bv,n	%r0(%rp)
	nop
EXIT(memcpy)
#endif /* MEMCPY */

#ifdef BCOPY
/*
 * void bcopy(const void *src, void *dst, size_t len);
 */
LEAF_ENTRY(bcopy)
	copy	%arg0, %r1
	copy	%arg1, %arg0
	copy	%r1, %arg1
	/* FALLTHROUGH */
#define _LABEL_F(l) __CONCAT(bcopy_F,l)
#define _LABEL_R(l) __CONCAT(bcopy_R,l)
#endif

#ifdef MEMMOVE
/*
 * void *memmove(void *dst, const void *src, size_t len);
 */
LEAF_ENTRY(memmove)
#define _LABEL_F(l) __CONCAT(memmove_F,l)
#define _LABEL_R(l) __CONCAT(memmove_R,l)
	copy	%arg0, %ret0
#endif /* MEMMOVE */

#if defined(BCOPY) || defined(MEMMOVE)

	/*
	 * If src >= dst or src + len <= dst, we copy
	 * forward, else we copy in reverse.
	 */
	add		%arg1, %arg2, %r1
	comb,>>=,n	%arg1, %arg0, 0
	comb,>>,n	%r1, %arg0, _LABEL_R(_go)

#define _LABEL _LABEL_F
	_COPY_FORWARD(%sr0,%arg1,%sr0,%arg0,%arg2)
#undef _LABEL

_LABEL_R(_go):
#define _LABEL _LABEL_R
	_COPY_REVERSE(%sr0,%arg1,%sr0,%arg0,%arg2)
#undef _LABEL

_LABEL_F(_done):
_LABEL_R(_done):
	bv,n	%r0(%rp)
	nop
#ifdef BCOPY
EXIT(bcopy)
#else
EXIT(memmove)
#endif
#endif /* BCOPY || MEMMOVE */
