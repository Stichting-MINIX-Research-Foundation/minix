/*	$NetBSD: crti.S,v 1.1 2013/07/13 18:52:35 skrll Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nick Hudson
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

RCSID("$NetBSD: crti.S,v 1.1 2013/07/13 18:52:35 skrll Exp $")

#include "sysident.S"

#define	MD_SECTION_PROLOGUE(sect, entry_pt)			\
	.section sect,"ax",@progbits				! \
	.global entry_pt					! \
	.proc							! \
	.callinfo frame=HPPA_FRAME_SIZE, calls, save_rp, save_sp, entry_gr=3 ! \
	.entry							! \
	entry_pt:						! \
	stw	%rp, HPPA_FRAME_CRP(%sp)			! \
	copy	%r3, %r1					! \
	copy	%sp, %r3					! \
	stw,ma	%r1, HPPA_FRAME_SIZE(%sp)			! \
	/* fall thru */						! \
	.exit							! \
	.procend						! \
	.previous

MD_SECTION_PROLOGUE(.init, _init)
MD_SECTION_PROLOGUE(.fini, _fini)
