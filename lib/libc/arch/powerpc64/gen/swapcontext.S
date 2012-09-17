/*	$NetBSD: swapcontext.S,v 1.4 2012/09/12 02:00:52 manu Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
	.text
	.asciz "$NetBSD: swapcontext.S,v 1.4 2012/09/12 02:00:52 manu Exp $"
#endif /* LIBC_SCCS && !lint */

ENTRY(swapcontext)
	mc_off	= 56				# offset of uc_mcontext
	stdu	%r1,-SF_SZ(%r1)			# set up new stack frame
	mflr	%r0
	std	%r0,[SF_SZ+SF_LR](%r1)		# save link register
	std	%r3,SF_PARAM(%r1)		# must save oucp (was 8)
	std	%r4,SF_PARAM+8(%r1)		# must save ucp (was 12)
	bl	._getcontext			# getcontext(oucp)
	nop
	cmpdi	%r3,0
	bne	1f
	ld	%r11,SF_PARAM(%r1)		# load oucp for adjustment
	ld	%r0,SF_SZ+SF_LR(%r1)		# get LR back
	std	%r0,mc_off+34*8(%r11)	# 	pc <- lr
	la	%r0,16(%r1)
	std	%r0,mc_off+1*8(%r11)		# adjust sp
	ld	%r3,SF_PARAM+8(%r1)		# load ucp
	bl	PIC_PLT(_C_LABEL(setcontext))	# setcontext(ucp)
	nop
1:
	ld	%r0,SF_SZ+SF_LR(%r1)
	mtlr	%r0
	la	%r1,SF_SZ(%r1)
	blr
