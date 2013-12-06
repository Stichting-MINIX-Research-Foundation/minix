/*	$NetBSD: swapcontext.S,v 1.7 2013/09/12 15:36:15 joerg Exp $	*/

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

__RCSID("$NetBSD: swapcontext.S,v 1.7 2013/09/12 15:36:15 joerg Exp $")

#define	XCALLFRAMELEN	(((2+3)*SZREG + CALLFRAMELEN - 1) & -CALLFRAMELEN)
#define	XCALLFRAME_R30	(XCALLFRAMELEN-1*SZREG)
#define	XCALLFRAME_UCP	(XCALLFRAMELEN-2*SZREG)
#define	XCALLFRAME_OUCP	(XCALLFRAMELEN-3*SZREG)

ENTRY(swapcontext)
	stwu	%r1,-XCALLFRAMELEN(%r1)		# set up new stack frame
	mflr	%r0
	stw	%r0,XCALLFRAMELEN+CALLFRAME_LR(%r1)	# save link register
	stw	%r3,XCALLFRAME_OUCP(%r1)	# must save oucp
	stw	%r4,XCALLFRAME_UCP(%r1)		# must save ucp
#ifdef __PIC__
	stw	%r30,XCALLFRAME_R30(%r1)	# must save r30
	PIC_TOCSETUP(swapcontext,%r30)		# setup toc pointer
#endif
	bl	PIC_PLT(_C_LABEL(_getcontext))	# getcontext(oucp)
	cmpwi	%r3,0
	bne	1f
	lwz	%r11,XCALLFRAME_OUCP(%r1)	# load oucp for adjustment
	lwz	%r0,XCALLFRAMELEN+CALLFRAME_LR(%r1)
	stw	%r0,UC_GREGS_PC(%r11)		# pc <- lr
	addi	%r0,%r1,XCALLFRAMELEN
	stw	%r0,UC_GREGS_R1(%r11)		# adjust sp
	lwz	%r3,XCALLFRAME_UCP(%r1)		# load ucp
	bl	PIC_PLT(_C_LABEL(setcontext))	# setcontext(ucp)
1:
	lwz	%r0,XCALLFRAMELEN+CALLFRAME_LR(%r1)
#ifdef __PIC__
	lwz	%r30,XCALLFRAME_R30(%r1)
#endif
	mtlr	%r0
	addi	%r1,%r1,XCALLFRAMELEN
	blr
END(swapcontext)
