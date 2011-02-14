/* $NetBSD: atomic_cas_8.S,v 1.1 2008/11/18 15:22:56 matt Exp $ */
/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>
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

RCSID("$NetBSD: atomic_cas_8.S,v 1.1 2008/11/18 15:22:56 matt Exp $")

ENTRY(atomic_cas_8)
	XPUSH	{r4,r5}			/* we need some more registers */
	and	r3, r0, #3		/* which byte do we replace? */
#if __ARMEB__
	eor	r3, r3, #3		/* bytes are reversed on BE */
#endif
	mov	r3, r3, lsl #3		/* multiply by 8 */
	mov	r1, r1, lsl r3		/* mov old value to correct byte */
	eor	r2, r1, r2, lsl r3	/* move new value to correct byte */
/*	eor	r2, r2, r1 */		/* new value is now (old ^ new) */
	mov	r5, #0xff		/* load mask */
	mov	r5, r5, lsl r3		/* and move to correct byte */
	mov	r3, r0			/* move pointer */

1:	ldrex	r4, [r3]		/* load 32bit value */
	and	r0, r4, r5		/* clear other bytes */
	teq	r0, r1			/* equal old value? */
	bne	2f			/*   nope, bail. */
	eor	r4, r4, r2		/* new == old ^ (old ^ new) */
	strex	ip, r4, [r3]		/* attempt to store it */
	cmp	ip, #0			/*   succeed? */
	bne	1b			/* nope, try again. */

2:	XPOP	{r4,r5}			/* don't need these anymore */
	and	r1, r3, #3
#if __ARMEB__
	eor	r1, r1, #3
#endif
	mov	r0, r0, lsr r1		/* shift it back to lsb byte */
	RET
