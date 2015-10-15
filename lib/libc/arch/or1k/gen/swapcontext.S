/*	$NetBSD: swapcontext.S,v 1.1 2014/09/03 19:34:25 matt Exp $	*/

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
#include "assym.h"

__RCSID("$NetBSD: swapcontext.S,v 1.1 2014/09/03 19:34:25 matt Exp $")

#define	CALLFRAME_LR	(-1*4)
#define	CALLFRAME_UCP	(-2*4)
#define	CALLFRAME_OUCP	(-3*4)
#define	CALLFRAME_GP	(-4*4)
#define	CALLFRAME_SP	(-5*4)
#define	CALLFRAME_LEN	(-CALLFRAME_SP)

ENTRY(swapcontext)
	l.sw	CALLFRAME_LR(r1),lr		# save link register
	l.sw	CALLFRAME_OUCP(r1),r3		# save oucp
	l.sw	CALLFRAME_UCP(r1),r4		# save ucp
	l.sw	CALLFRAME_SP(r1),r1		# set up stack chain
#ifdef __PIC__
	l.sw	CALLFRAME_GP(r1),r16		# save r16
	PIC_GOTSETUP(r16)			# setup got pointer
#endif
	l.addi	r1,r1,-CALLFRAME_LEN		# adjust stack
	l.jal	PLT(_C_LABEL(_getcontext))	# getcontext(oucp)
	l.nop					# -- delay slot --
	l.sfeqi	r11,0				# error?
	l.bnf	1f				#   yes, cleanup
	l.nop					#   -- delay slot --
	l.addi	r5,r1,CALLFRAME_LEN		# adjust stack
	l.lwz	r4,CALLFRAME_OUCP(r5)		# load oucp for adjustment
	l.sw	UC_GREGS_PC(r4),lr		# pc <- lr
	l.sw	UC_GREGS_SP(r4),r1		# adjust sp
	l.lwz	r3,CALLFRAME_UCP(r5)		# load ucp
	# can't do a tail call due to needing r16 for PLT
	l.jal	PLT(_C_LABEL(setcontext))	# setcontext(ucp)
	l.nop					# -- delay slot --
1:
	l.addi	r1,r1,CALLFRAME_LEN		# restore stack
#ifdef __PIC__
	l.lwz	r16,CALLFRAME_GP(r1)		# restore r16
#endif
	l.lwz	lr,CALLFRAME_LR(r1)		# refetch lr
	l.jr	lr				# simply return
	l.nop					# -- delay slot --
END(swapcontext)
