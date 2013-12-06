/*      $NetBSD: strncmp.S,v 1.2 2013/08/19 00:57:25 matt Exp $ */

/*
 * Copyright (c) 2002 ARM Ltd
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
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ARM LTD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ARM LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

RCSID("$NetBSD: strncmp.S,v 1.2 2013/08/19 00:57:25 matt Exp $")

ENTRY(strncmp)
/* if ((len - 1) < 0) return 0 */
	subs	r2, r2, #1
#ifdef __thumb__
	bmi	5f
#else
	movmi	r0, #0
	RETc(mi)
#endif

/* ip == last src address to compare */
#ifdef __thumb__
	adds	r3, r0, r2
	mov	ip, r3
	subs	r1, r1, r0
#else
	add	ip, r0, r2
#endif
1:
#ifdef __thumb__
	ldrb	r2, [r0]
	ldrb	r3, [r1, r0]
	adds	r0, r0, #1
	cmp	ip, r0		/* exhausted length? */
	bcc	4f		/*   yes, we're done */
	cmp	r2, #1		/* NUL? */
	bcc	4f		/*   yes, we're done */
	cmp	r2, r3		/* different? */
#else
	ldrb	r2, [r0], #1
	ldrb	r3, [r1], #1
	cmp	ip, r0
	cmpcs	r2, #1
	cmpcs	r2, r3
#endif
	beq	1b
4:	subs	r0, r2, r3
	RET
#ifdef __thumb__
5:	movs	r0, #0
	RET
#endif
END(strncmp)
