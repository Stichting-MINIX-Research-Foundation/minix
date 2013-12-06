/*	$NetBSD: memset_arm.S,v 1.2 2013/01/14 19:15:13 matt Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#if defined(NEON)
#define	STORE8		vst1.32		{d0}, [ip:64]!
#define	STORE16		vst1.32		{d0-d1}, [ip:64]!
#define	STORE32		vst1.32		{d0-d3}, [ip:64]!
#elif defined(VFP)
#define	STORE8		vstmia		ip!, {d0}
#define	STORE16		vstmia		ip!, {d0-d1}
#define	STORE32		vstmia		ip!, {d0-d3}
#elif defined(_ARM_ARCH_DWORD_OK)
#define	STORE8		strd		r2, [ip], #8
#define	STORE16		STORE8; STORE8
#define	STORE32		STORE16; STORE16
#else
#define	STORE8		stmia		ip!, {r2,r3}
#define	STORE16		STORE8; STORE8
#define	STORE32		STORE16; STORE16
#endif
/*
 * memset: Sets a block of memory to the specified value
 * Using NEON instructions
 *
 * On entry:
 *   r0 - dest address
 *   r1 - byte to write
 *   r2 - number of bytes to write
 *
 * On exit:
 *   r0 - dest address
 */
/* LINTSTUB: Func: void *memset(void *, int, size_t) */
ENTRY(memset)
	ands		r3, r1, #0xff	/* We deal with bytes */
	orrne		r3, r3, r3, lsl #8	/* replicate to all bytes */
	orrne		r3, r3, r3, lsl #16	/* replicate to all bytes */
	movs		r1, r2		/* we need r2 & r3 */
	RETc(eq)			/* return if length is 0 */
	mov		ip, r0		/* r0 needs to stay the same */

	cmp		r1, #12		/* is this a small memset? *?
	blt		.Lbyte_by_byte	/*   then do it byte by byte */

	/* Ok first we will dword align the address */
	ands		r2, ip, #7	/* grab the bottom three bits */
	beq		.Lmemset_dwordaligned	/* The addr is dword aligned */

	rsb		r2, r2, #8	/* how far until dword aligned? */
	sub		r1, r1, r2	/* subtract it from remaining length */
	mov		r2, r3		/* duplicate fill value */

	tst		ip, #1		/* halfword aligned? */
	strneb		r3, [ip], #1	/*   no, write a byte */
	tst		ip, #2		/* word aligned? */
	strneh		r3, [ip], #2	/*   no, write a halfword */
	tst		ip, #4		/* dword aligned? */
	strne		r3, [ip], #4	/*   no, write a word */

	/* We are now doubleword aligned */
.Lmemset_dwordaligned:
#if defined(NEON)
	vdup.8		q0, r3		/* move fill to SIMD */
	vmov		q1, q0		/* put fill in q1 (d2-d3) */
#elif defined(VFP)
	mov		r2, r3		/* duplicate fill value */
	vmov		d0, r2, r3	/* move to VFP */
	vmov		d1, r2, r3
	vmov		d2, r2, r3
	vmov		d3, r2, r3
#endif

#if 1
	cmp		r1, #128
	blt		.Lmemset_mainloop
	ands		r2, ip, #63	/* check for 64-byte alignment */
	beq		.Lmemset_mainloop
	/*
	 * Let's align to a 64-byte boundary so that stores don't cross
	 * cacheline boundaries.  We also know we have at least 128-bytes to
	 * copy so we don't have to worry about the length at the moment.
	 */
	rsb		r2, r2, #64	/* how many bytes until 64 bytes */
	sub		r1, r1, r2	/* subtract from remaining length */
#if !defined(NEON) && !defined(VFP)
	mov		r2, r3		/* put fill back in r2 */
#endif

	tst		ip, #8		/* quadword aligned? */
	beq		1f		/*   yes */
	STORE8				/*   no, store a dword */
1:	tst		ip, #16		/* octaword aligned? *?
	beq		2f		/*   yes */
	STORE16				/*   no, store a quadword */
2:	tst		ip, #32		/* 32 word aligned? */
	beq		.Lmemset_mainloop		/*   yes */
	STORE32				/*   no, make 64-byte aligned */
#endif

.Lmemset_mainloop:
#if !defined(NEON) && !defined(VFP)
	mov		r2, r3		/* put fill back in r2 */
#endif
	subs		r1, r1, #64	/* subtract an initial 64 */
	blt		.Lmemset_lessthan_64bytes

3:	STORE32				/* store first octaword */
	STORE32				/* store second octaword */
	RETc(eq)			/* return if done */
	subs		r1, r1, #64	/* subtract another 64 */
	bge		3b		/* and do other if still >= 0 */
.Lmemset_lessthan_64bytes:
	tst		r1, #32		/* do we have 16 bytes left? */
	beq		.Lmemset_lessthan_32bytes
	STORE32				/*    yes, store an octaword */
	bics		r1, r1, #32	/* subtract 16 */
	RETc(eq)			/* return if length is 0 */
.Lmemset_lessthan_32bytes:
	tst		r1, #16		/* do we have 16 bytes left? */
	beq		.Lmemset_lessthan_16bytes
	STORE16				/*   yes, store a quadword */
	bics		r1, r1, #16	/* subtract 16 */
	RETc(eq)			/* return if length is 0 */
.Lmemset_lessthan_16bytes:
	tst		r1, #8		/* do we have 8 bytes left? */
	beq		.Lmemset_lessthan_8bytes/*   no */
	STORE8				/*   yes, store a dword */
	bics		r1, r1, #8	/* subtract 8 */
	RETc(eq)			/* return if length is 0 */
.Lmemset_lessthan_8bytes:
	tst		r1, #4		/* do we have a word left? */
	strne		r2, [ip], #4	/*   yes, so write one */
	tst		r1, #2		/* do we have a halfword left? */
	strneh		r2, [ip], #2	/*   yes, so write one */
	tst		r1, #1		/* do we have a byte left? */
	strneb		r2, [ip], #1	/*   yes, so write one */
	RET				/* return */

.Lbyte_by_byte:
	subs		r1, r1, #1	/* can we write a byte? */
	RETc(lt)			/*   no, we're done */
	strb		r3, [ip], #1	/*   yes, so do it */
	b		.Lbyte_by_byte	/* try next byte */
END(memset)
