/*	$NetBSD: ashiftrt.S,v 1.7 2009/01/07 22:15:18 uwe Exp $	*/

/*-
 * Copyright (C) 1999 Tsubai Masanari.  All rights reserved.
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

#include <machine/asm.h>


#ifdef __ELF__
	.hidden __ashiftrt_r4_0
	.hidden __ashiftrt_r4_1
	.hidden __ashiftrt_r4_2
	.hidden __ashiftrt_r4_3
	.hidden __ashiftrt_r4_4
	.hidden __ashiftrt_r4_5
	.hidden __ashiftrt_r4_6
	.hidden __ashiftrt_r4_7
	.hidden __ashiftrt_r4_8
	.hidden __ashiftrt_r4_9
	.hidden __ashiftrt_r4_10
	.hidden __ashiftrt_r4_11
	.hidden __ashiftrt_r4_12
	.hidden __ashiftrt_r4_13
	.hidden __ashiftrt_r4_14
	.hidden __ashiftrt_r4_15
	.hidden __ashiftrt_r4_16
	.hidden __ashiftrt_r4_17
	.hidden __ashiftrt_r4_18
	.hidden __ashiftrt_r4_19
	.hidden __ashiftrt_r4_20
	.hidden __ashiftrt_r4_21
	.hidden __ashiftrt_r4_22
	.hidden __ashiftrt_r4_23
	.hidden __ashiftrt_r4_24
	.hidden __ashiftrt_r4_25
	.hidden __ashiftrt_r4_26
	.hidden __ashiftrt_r4_27
	.hidden __ashiftrt_r4_28
	.hidden __ashiftrt_r4_29
	.hidden __ashiftrt_r4_30
	.hidden __ashiftrt_r4_31
#endif  /* __ELF__ */


NENTRY(__ashiftrt_r4_31)
	shll	r4
	rts
	 subc	r4, r4

ALTENTRY(__ashiftrt_r4_30)
	shar	r4

ALTENTRY(__ashiftrt_r4_29)
	shar	r4

ALTENTRY(__ashiftrt_r4_28)
	shar	r4

ALTENTRY(__ashiftrt_r4_27)
	shar	r4

ALTENTRY(__ashiftrt_r4_26)
	shar	r4

ALTENTRY(__ashiftrt_r4_25)
	shar	r4

ALTENTRY(__ashiftrt_r4_24)
	shlr16	r4
	shlr8	r4
	rts
	 exts.b	r4, r4

ALTENTRY(__ashiftrt_r4_23)
	shar	r4

ALTENTRY(__ashiftrt_r4_22)
	shar	r4

ALTENTRY(__ashiftrt_r4_21)
	shar	r4

ALTENTRY(__ashiftrt_r4_20)
	shar	r4

ALTENTRY(__ashiftrt_r4_19)
	shar	r4

ALTENTRY(__ashiftrt_r4_18)
	shar	r4

ALTENTRY(__ashiftrt_r4_17)
	shar	r4

ALTENTRY(__ashiftrt_r4_16)
	shlr16	r4
	rts
	 exts.w	r4, r4

ALTENTRY(__ashiftrt_r4_15)
	swap.w	r4, r4
	cmp/pz	r4
	bf/s	1f
	 exts.w	r4, r4
	rts
	 shll	r4
1:
	sett
	rts
	 rotcl	r4

ALTENTRY(__ashiftrt_r4_14)
	shar	r4

ALTENTRY(__ashiftrt_r4_13)
	shar	r4

ALTENTRY(__ashiftrt_r4_12)
	shar	r4

ALTENTRY(__ashiftrt_r4_11)
	shar	r4

ALTENTRY(__ashiftrt_r4_10)
	shar	r4

ALTENTRY(__ashiftrt_r4_9)
	shar	r4

ALTENTRY(__ashiftrt_r4_8)
	shar	r4

ALTENTRY(__ashiftrt_r4_7)
	shar	r4

ALTENTRY(__ashiftrt_r4_6)
	shar	r4

ALTENTRY(__ashiftrt_r4_5)
	shar	r4

ALTENTRY(__ashiftrt_r4_4)
	shar	r4

ALTENTRY(__ashiftrt_r4_3)
	shar	r4

ALTENTRY(__ashiftrt_r4_2)
	shar	r4

ALTENTRY(__ashiftrt_r4_1)
	rts
	 shar	r4

ALTENTRY(__ashiftrt_r4_0)
	rts
	 nop
