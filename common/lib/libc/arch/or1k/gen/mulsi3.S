/* $NetBSD: mulsi3.S,v 1.1 2014/09/03 19:34:25 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
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

__RCSID("$NetBSD: mulsi3.S,v 1.1 2014/09/03 19:34:25 matt Exp $")

ENTRY(__mulsi3)
	l.movhi	r11, 0

	l.sfeqi	r3, 0
	l.bf	.Lret
	l.nop

	l.sfeqi	r4, 0
	l.bf	.Lret
	l.nop

	l.sfleu	r3, r4		# is r3 < r4 (unsigned)
#if 0
	l.ori	r6, r3, 0	# move r3 to r6
	l.bf	.Lloop		#   yes, r6 already has smaller value, 
	l.nop
	l.ori	r6, r4, 0	# move r4 to r6 because it's smaller than r3
#else
	l.cmov	r6, r3, r4	# choose lesser of r3 or r4
#endif

.Lloop:
	l.andi	r5, r6, 1	# get LSB
	l.sub	r8, r0, r5	# 0 -> 0, 1 -> 0xffffffff
	l.and	r8, r8, r4	# r8 = r6 & 1 ? r4 : 0
	l.add	r11, r11, r8	# add to result
	l.srli	r4, r4, 1	# discard LSB
	l.slli	r6, r6, 1	# multiply by 2
.Lentry:
	l.sfeqi	r4, 0		# only 0 left?
	l.bf	.Lret		#   yes, we're done
	l.nop
	l.sfeqi	r6, 0		# only 0 left?
	l.bnf	.Lloop		#   no, another round of long multiplication.
	l.nop

.Lret:	l.jr	lr
	l.nop
END(__mulsi3)
