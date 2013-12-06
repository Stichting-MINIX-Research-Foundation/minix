/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#include <machine/asm.h>

RCSID("$NetBSD: strcpy_arm.S,v 1.3 2013/08/11 04:56:32 matt Exp $")

#ifdef STRLCPY
#ifdef _LIBC
WEAK_ALIAS(strlcpy, _strlcpy)
#endif
#define	FUNCNAME	strlcpy
#elif defined(STRNCPY)
#define	FUNCNAME	strncpy
#else
#define	FUNCNAME	strcpy
#endif

#ifdef _LIBC
#include "namespace.h"
#endif

#ifdef __ARMEL__
#define	lslo	lsr		/* shift to lower address */
#define	lshi	lsl		/* shift to higher address */
#define	BYTE0	0x000000ff
#define	BYTE1	0x0000ff00
#define	BYTE2	0x00ff0000
#define	BYTE3	0xff000000
#else
#define	lslo	lsl		/* shift to lower address */
#define	lshi	lsr		/* shift to higher address */
#define	BYTE0	0xff000000
#define	BYTE1	0x00ff0000
#define	BYTE2	0x0000ff00
#define	BYTE3	0x000000ff
#endif

/*
 * On armv6 and later, to quickly determine if a word contains a NUL (0) byte,
 * we add 254 to each byte using the UQADD8 (unsigned saturating add 8)
 * instruction.  For every non-NUL byte, the result for that byte will become
 * 255.  For NUL, it will be 254.  When we complement the result of all 4 adds,
 * if the result is non-0 then we must have encountered a NUL.
 *
 * For earlier architecture, we just use tst on all 4 bytes.  There are other
 * algorithms to detect NULs but they take longer and use more instructions.
 */

/*
 * char *strcpy(char *dst, const char *src);
 * char *strncpy(char *dst, const char *src, size_t len);
 * size_t strlcpy(char *dst, const char *src, size_t len);
 */

	.text
ENTRY(FUNCNAME)
#if defined(STRLCPY)
	cmp	r2, #1			/* is length 1 or less? */
	bhi	1f			/*   no, do normal */
	moveq	r3, #0			/*   = 1? load NUL */
	strbeq	r3, [r0]		/*   = 1? write NUL to dst */
	mov	r0, r1			/* move src to r0 */
	b	PLT_SYM(_C_LABEL(strlen)) /* and tailcall strlen */
1:
	sub	r2, r2, #1		/* leave one byte for NUL */
#endif
#if defined(STRNCPY)
	cmp	r2, #0			/* 0 length? */
	RETc(eq)			/*   yes, just return */
#endif
	push	{r4-r9}			/* save some registers */
#ifdef _ARM_ARCH_6
#ifdef _ARM_ARCH_7
	movw	r7, #0xfefe		/* magic constant; 254 in each byte */
#else
	mov	r7, #0xfe		/* put 254 in low byte */
	orr	r7, r7, r7, lsl #8	/* move to next byte */
#endif
	orr	r7, r7, r7, lsl #16	/* move to next halfword */
#endif

#if defined(STRLCPY)
	add	r6, r1, #1		/* save for return (deal with NUL) */
#else
	mov	r6, r0			/* save for return */
#endif

.Ldst_align:
	tst	r0, #3			/* check for dst alignment */
	beq	.Ldst_aligned		/*   ok, proceed to next check */
	ldrb	r5, [r1], #1		/* load a byte */
#if defined(STRNCPY)
	subs	r2, r2, #1		/* subtract out from count */
	bmi	.Ldst_full		/*   zero? the dst has no more room */
#endif
	strb	r5, [r0], #1		/* store a byte */
	teq	r5, #0			/* was it a NUL? */
	beq	.Lend_of_string		/*   yes, we are done */
#if defined(STRLCPY)
	subs	r2, r2, #1		/* subtract one from count */
	strbeq	r2, [r0], #1		/*    zero? write trailing NUL */
	beq	.Ldst_full		/*    zero? the dst has no more room */
#endif
	b	.Ldst_align		/* loop around for next byte */
.Ldst_aligned:
	tst	r1, #3			/* get the misalignment of src */
	bne	.Lincongruent		/*  !=? incongruent (slower) */

	/*   =?   congruent (faster) */

.Lcongruent:
#if defined(STRLCPY)
	add	r6, r6, #3		/* compensate for word post-inc */
#endif
	b	.Lcongruent_mainloop_load
.Lcongruent_mainloop:
#if defined(STRLCPY) || defined(STRNCPY)
	subs	r2, r2, #4		/* subtract 4 from the count */
	bmi	.Lno_more_room
#endif
	str	r5, [r0], #4		/* store word into dst */
#if defined(STRLCPY)
	beq	.Lno_more_room		/*   count is 0? no room in dst */
#endif
#if defined(STRNCPY)
	beq	.Ldst_full_word_aligned	/*   count is 0? no room in dst */
#endif
.Lcongruent_mainloop_load:
	ldr	r5, [r1], #4		/* load word from source */
#if defined(_ARM_ARCH_6)
	uqadd8	r3, r5, r7		/* magic happens here */
	mvns	r3, r3			/* is the complemented result 0? */
	beq	.Lcongruent_mainloop	/*    yes, no NULs, do it again */
#else
	tst	r5, #BYTE0		/* does byte 0 contain a NUL? */
	tstne	r5, #BYTE1		/*   no, does byte 1 contain a NUL? */
	tstne	r5, #BYTE2		/*   no, does byte 2 contain a NUL? */
	tstne	r5, #BYTE3		/*   no, does byte 3 contain a NUL? */
	bne	.Lcongruent_mainloop	/*    yes, no NULs, do it again */
#endif
#if defined(STRLCPY) && 0
	sub	r1, r1, #3		/* back up src pointer */
#endif
#if defined(_ARM_ARCH_6)
#ifdef __ARMEL__
	rev	r3, r3			/* CLZ needs BE data */
#endif
	clz	r3, r3			/* count leading zeros */
#else
	mov	r3, #0			/* assume NUL is in byte 0 */
	tst	r5, #BYTE0		/* is NUL in byte 2? */
	beq	.Lcongruent_last_bytes	/*   yes, done searching. */
	mov	r3, #8			/* assume NUL is in byte 1 */
	tst	r5, #BYTE1		/* is NUL in byte 2? */
	beq	.Lcongruent_last_bytes	/*   yes, done searching. */
	mov	r3, #16			/* assume NUL is in byte 2 */
	tst	r5, #BYTE2		/* is NUL in byte 2? */
#if !defined(STRLCPY)
	beq	.Lcongruent_last_bytes	/*   yes, done searching. */
	mov	r3, #24			/* NUL must be in byte 3 */
#else
	movne	r3, #24			/*    no, then NUL is in byte 3 */
#endif
#endif /* _ARM_ARCH_6 */
#if defined(STRLCPY)
.Lcongruent_last_bytes:
#endif
#if defined(STRLCPY)
	add	r1, r1, r3, lsr #3	/* position to point at NUL + 4 */
#endif
	b	.Llast_bytes		/* store the last bytes */


.Lincongruent:
	/*
	 * At this point dst is word aligned by src is not.  Read bytes
	 * from src until it is read aligned.
	 */
	and	r3, r1, #3		/* extract misalignment */
	mov	r9, r3, lsl #3		/* calculate discard shift */
	rsb	r8, r9, #32		/* calculate insertion shift */
#if defined(STRLCPY)
	add	r6, r6, #3		/* compensate for word post-inc */	
#endif
	bic	r1, r1, #3		/* word align src */
	ldr	r5, [r1], #4		/* load word frm src */
	mov	r4, r5, lslo r9		/* discard lo bytes from src */
	tst	r4, #BYTE0		/* does byte 0 contain a NUL? */
#if defined(STRNCPY)
	beq	.Lend_of_string		/*   yes, zero fill rest of string */
#else
	moveq	r3, r9			/*   yes, set offset */
	beq	.Lincongruent_end_of_string /*   yes, deal with the last bytes */
#endif
	/*
	 * To make our test for NULs below do not generate false positives,
	 * fill the bytes in the word we don't want to match with all 1s.
	 */
	mvn	r3, #0			/* create a mask */
	mov	r3, r3, lslo r8		/* zero out bytes being kept */
	orr	r5, r5, r3		/* merge src and mask */
#ifdef _ARM_ARCH_6
	uqadd8	r3, r5, r7		/* NUL detection magic happens */
	mvns	r3, r3			/* is the complemented result 0? */
	beq	.Lincongruent_mainloop_load /*   yes, no NUL encountered! */
#ifdef __ARMEL__
	rev	r3, r3			/* CLZ wants BE input */
#endif
	clz	r3, r3			/* count leading zeros */
#else
	/*
	 * We already tested for byte 0 above so we don't need to it again.
	 */
	mov	r3, #24			/* assume NUL is in byte 3 */
	tst	r5, #BYTE1		/* did we find a NUL in byte 1? */
	subeq	r3, r3, #8		/*   yes, decremnt byte position */
	tstne	r5, #BYTE2		/*   no, did we find a NUL in byte 2? */
	subeq	r3, r3, #8		/*   yes, decremnt byte position */
	tstne	r5, #BYTE3		/*   no, did we find a NUL in byte 3? */
	bne	.Lincongruent_mainloop_load /*   no, no NUL encountered! */
#endif
	mov	r5, r4			/* discard already dealt with bytes */
.Lincongruent_end_of_string:
#if defined(STRLCPY)
	add	r1, r1, r3, lsr #3	/* then add offset to NUL */
#endif
	sub	r3, r3, r9		/* adjust NUL offset */
	b	.Llast_bytes		/* NUL encountered! finish up */

#if defined(STRLCPY) || defined(STRNCPY)
.Lincongruent_no_more_room:
	mov	r5, r4			/* move data to be stored to r5 */
	b	.Lno_more_room		/* fill remaining space */
#endif /* STRLCPY || STRNCPY */

	/*
	 * At this point both dst and src are word aligned and r4 contains
	 * partial contents from src.
	 */
.Lincongruent_mainloop:
	orr	r4, r4, r5, lshi r8	/* put new src data into dst word */
#if defined(STRLCPY) || defined(STRNCPY)
	subs	r2, r2, #4		/* subtract 4 from count */
	bmi	.Lincongruent_no_more_room /*   count < 0? dst will be full */
#endif
	str	r4, [r0], #4		/* store word in dst */
#if defined(STRLCPY)
	beq	.Lno_more_room		/*   space left is 0? stop copy */
#endif
#if defined(STRNCPY)
	beq	.Ldst_full_word_aligned	/*   space left is 0? stop copy */
#endif
	mov	r4, r5, lslo r9		/* move rest of src into dst word */
.Lincongruent_mainloop_load:
	ldr	r5, [r1], #4		/* read src */
#ifdef _ARM_ARCH_6
	uqadd8	r3, r5, r7		/* magic happens here */
	mvns	r3, r3			/* is the complemented result 0? */
	beq	.Lincongruent_mainloop	/*   yes, no NUL encountered! */
	/*
	 * fall into this since we encountered a NULL.  At this point we have
	 * from 1-5 bytes (excluding trailing NUL) to write.
	 */
#ifdef __ARMEL__
	rev	r3, r3			/* CLZ works on BE data */
#endif
	clz	r3, r3			/* count leading zeroes */
#else
	tst	r5, #BYTE0		/* does byte 0 contain a NUL? */
	tstne	r5, #BYTE1		/*   no, does byte 1 contain a NUL? */
	tstne	r5, #BYTE2		/*   no, does byte 2 contain a NUL? */
	tstne	r5, #BYTE3		/*   no, does byte 3 contain a NUL? */
	bne	.Lincongruent_mainloop	/*   no, no NUL encountered! */
	/*
	 * fall into this since we encountered a NULL.  At this point we have
	 * from 1-5 bytes (excluding trailing NUL) to write.
	 */
	mov	r3, #0			/* assume a NUL is in byte 0 */
	tst	r5, #BYTE0		/* is there a NUL in byte 0? */
	beq	1f			/*   yes, found a NUL! */
	mov	r3, #8			/* assume a NUL is in byte 1 */
	tst	r5, #BYTE1		/* is there a NUL in byte 0? */
	beq	1f			/*   yes, found a NUL! */
	tst	r5, #BYTE2		/* is there a NUL in byte 2? */
	moveq	r3, #16			/*   yes, mark its position */
	movne	r3, #24			/*   no, it must be in byte 3 */
1:
#endif
	orr	r4, r4, r5, lshi r8	/* merge new and old src words */
#if defined(STRLCPY)
	add	r1, r1, r3, lsr #3	/* adjust src to point to NUL */
#endif
	add	r3, r3, r8		/* add remainder bytes worth */
	cmp	r3, #32			/* do we have at least one word to write? */
	movlt	r5, r4			/*   no, move source bytes to expected reg */
	blt	.Llast_bytes		/*   no, deal with them */
#if defined(STRLCPY)
	subs	r2, r2, #4		/* subtract 4 from count */
	bpl	1f			/*   we have space for at least 4 */
	/*
	 * Since the space just went minus, we don't have enough room to
	 * write all 4 bytes.  In fact, the most we can write is 3 so just
	 * just lie and say we have 3 bytes to write and discard the rest.
	 */
	add	r2, r2, #4		/* add 4 back */
	mov	r3, #24			/* say we have 3 bytes */
	mov	r5, r4			/* discard the bytes we can't store */
	b	.Llast_bytes		/* and treat this as our last word */
1:
#elif defined(STRNCPY)
	subs	r2, r2, #4		/* subtract 4 from count */
	bmi	.Lincongruent_no_more_room /*   count < 0? dst will be full */
#endif
	str	r4, [r0], #4		/* store dst word */
#if defined(STRNCPY)
	beq	.Ldst_full_word_aligned	/*   space left is 0? stop copy */
#endif
#if defined(STRLCPY)
	bne	1f			/* we still have space remaining */
	strb	r2, [r0]		/* write final NUL */
	b	.Lend_of_string		/* we are done */
1:
#endif
	/*
	 * Subtract the 32 bits just written from the number of bits left
	 * to write.  If 0 bits are left and not doing strncpy, just write
	 * the trailing NUL and be done.
	 */
	subs	r3, r3, #32		/* we wrote one word */
#if !defined(STRNCPY)
	bne	1f			/* no more data? */
	strb	r3, [r0]		/* write final NUL */
	b	.Lend_of_string		/* we are done */
1:
#endif
	/*
	 * At this point after writing 4 bytes, we have 0 or 1 bytes left to
	 * write (excluding the trailing NUL).
	 */
	mov	r5, r5, lslo r9		/* get remainder of src */

	/* fall into .Llast_bytes */

#if !defined(STRLCPY)
.Lcongruent_last_bytes:
#endif
.Llast_bytes:
	/*
	 * r5 contains the last word and is in host byte order.
	 * r3 contains number of bits left to copy (0..31).
	 * r1 should point to the NUL + 4.
	 */
	bics	ip, r3, #7		/* truncate bits, is result 0? */
#if !defined(STRNCPY)
	bne	1f			/*   no, have to write some bytes */
	strb	ip, [r0]		/*   yes, write trailing NUL */
	b	.Lend_of_string		/*   yes, and we are the end */
1:
#endif
#if defined(STRLCPY) || defined(STRNCPY)
	cmp	r2, ip, lsr #3		/* is there enough room? */
	movlt	ip, r2, lsl #3		/*   no, only fill remaining space */
#endif
	mvn	r3, #0			/* create a mask */
	mov	r3, r3, lshi ip		/* clear leading bytes */
	bic	r5, r5, r3		/* clear trailing bytes */
#if defined(STRNCPY)
	cmp	r2, #4			/* room for 4 bytes? */
	movge	ip, #32			/*   yes, we will write 4 bytes */
	bge	2f			/*   yes, and go do it */
	mvn	r3, #0			/* create a mask (again) */
	mov	ip, r2, lsl #3		/* remaining space bytes -> bits */
	mov	r3, r3, lshi ip		/* clear remaining bytes */
#elif defined(STRLCPY)
	cmp	r2, #3			/* do we have room for 3 bytes & NUL? */
	bge	2f			/*   yes, just clear out dst */
	mov	r3, r3, lshi #8		/* mask out trailing NUL */
#else
	cmp	ip, #24			/* are we writing 3 bytes & a NUL? */
	bge	2f			/*   yes, just overwrite dst */
	mov	r3, r3, lshi #8		/* mask out trailing NUL */
#endif /* !STRNCPY */
	ldr	r4, [r0]		/* fetch dst word */
	and	r4, r4, r3		/* preserve trailing bytes */
	orr	r5, r5, r4		/* merge dst with src */
2:	str	r5, [r0], #4		/* store last word */
#if defined(STRNCPY)
	subs	r2, r2, ip, lsr #3	/* subtract bytes cleared from count */
	beq	.Ldst_full_word_aligned
#endif
	b	.Lend_of_string

#if defined(STRLCPY) || defined(STRNCPY)
.Lno_more_room:
#if defined(STRLCPY)
	cmp	r2, #-1			/* tried to write 3 bytes? */
	blt	1f			/*   less, partial word write */
	cmp	r2, #0			/* no space left? */
	strbeq	r2, [r0]		/* write the final NUL */
	bicne	r5, r5, #BYTE3		/* clear trailing NUL */
	strne	r5, [r0]		/* write last word */
	b	.Ldst_full_word_aligned	/* the dst buffer is full */
1:
#endif /* STRLCPY */
	add	r2, r2, #4		/* restore remaining space */
	ldr	r4, [r0]		/* load dst */
	mvn	r3, #0			/* create a mask */
	mov	r2, r2, lsl #3		/* bytes -> bits */
	mov	r3, r3, lshi r2		/* clear leading bytes */
	bic	r5, r5, r3		/* clear trailing bytes from src */
#if defined(STRLCPY)
	mov	r3, r3, lshi #8		/* mask out trailing NUL */
#endif /* STRLCPY */
	and	r4, r4, r3		/* preserve trailing bytes in dst */
	orr	r4, r4, r5		/* merge src with dst */
	str	r4, [r0], #4		/* write last word */
	b	.Ldst_full_word_aligned
#endif /* STRLCPY || STRNCPY */

#if defined(STRLCPY)
	/*
	 * Destination was filled (and NUL terminated).
	 * All that's left is count the number of bytes left in src.
	 */
.Ldst_full:
1:	tst	r1, #3			/* dst word aligned? */
	beq	2f			/*   yes, so do it word by word */
	ldrb	r5, [r1], #1		/* load next byte */
	teq	r5, #0			/* is it a NUL? */
	bne	1b			/*   no, check alignment */
	b	.Lend_of_string		/* and return */
2:	add	r6, r6, #3		/* compensate for post-inc */
.Ldst_full_word_aligned:
3:	ldr	r5, [r1], #4		/* load word from src */
#ifdef _ARM_ARCH_6
	uqadd8	r5, r5, r7		/* perform NUL magic */
	mvns	r5, r5			/* complement all 0s? */
	beq	3b			/*   yes, no NUL so get next word */
#else
	tst	r5, #BYTE0		/* does byte 0 contain a NUL? */
	tstne	r5, #BYTE1		/*   no, does byte 1 contain a NUL? */
	tstne	r5, #BYTE2		/*   no, does byte 2 contain a NUL? */
	tstne	r5, #BYTE3		/*   no, does byte 3 contain a NUL? */
	bne	3b			/*   no, no NUL encountered! */
#endif
#ifdef _ARM_ARCH_6
#ifdef __ARMEL__
	rev	r5, r5			/* CLZ needs BE data */
#endif
	clz	r5, r5			/* count leading zeros */
	add	r1, r1, r5, lsr #3	/* add offset to NUL to src pointer */
#else
	tst	r5, #BYTE0		/* is there a NUL in byte 0? */
	beq	4f			/*   yes, don't check any further */
	add	r1, r1, #1		/*   no, advance src pointer by 1 */
	tst	r5, #BYTE1		/* is there a NUL in byte 1? */
	beq	4f			/*   yes, don't check any further */
	add	r1, r1, #1		/*   no, advance src pointer by 1 */
	tst	r5, #BYTE2		/* is there a NUL in byte 2? */
	addne	r1, r1, #1		/*   no, there must be in byte 3 */
4:
#endif /* _ARM_ARCH_6 */
.Lend_of_string:
	sub	r0, r1, r6		/* subtract start from finish */
	pop	{r4-r9}			/* restore registers */
	RET
#elif defined(STRNCPY)
.Lend_of_string:
	teq	r2, #0			/* any bytes left to zero? */
	beq	3f 			/*   no, just return. */
	mov	r1, #0			/*   yes, prepare to zero */
	cmp	r2, #16			/* some, but not a lot? */
	ble	1f
	mov	r4, lr			/* preserve lr */
	bl	PLT_SYM(_C_LABEL(memset)) /*   yes, and let memset do it */
	mov	lr, r4			/* restore lr */
	b	3f			/* return */
1:	add	ip, r0, r2		/* calculate stopping point */
2:	strb	r1, [r0], #1		/* clear a byte */
	cmp	r0, ip			/* done? */
	blt	2b			/*   no, clear next byte */
3:	mov	r0, r6			/* restore dst pointer */
	pop	{r4-r9}			/* restore registers */
	RET
.Ldst_full:
.Ldst_full_word_aligned:
	/*
	 * Destination was filled (but not NUL terminated).
	 * All that's left is return the start of dst
	 */
	mov	r0, r6			/* restore dst pointer */
	pop	{r4-r9}			/* restore registers */
	RET
#else
.Lend_of_string:
	mov	r0, r6			/* restore dst pointer */
	pop	{r4-r9}			/* restore registers */
	RET
#endif
END(FUNCNAME)
