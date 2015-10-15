/*	$NetBSD: fpsetmask.S,v 1.7 2014/01/23 00:00:16 joerg Exp $	*/

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

#ifndef __VFP_FP__
#error FPA is not supported anymore
#endif

#include <arm/asm.h>
#include <arm/vfpreg.h>

/*
 * Set the current FP exception enable mask
 */

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpsetmask, _fpsetmask)
#define	fpsetmask	_fpsetmask
#endif

ENTRY(fpsetmask)
	and	r2, r0, #VFP_FPSCR_CSUM
	vmrs	r1, fpscr
	mov	r0, r1, lsr #8		/* Return old mask */
	bic	r1, r1, #VFP_FPSCR_ESUM
	orr	r1, r1, r2, lsl #8
	vmsr	fpscr, r1
	and	r0, r0, #VFP_FPSCR_CSUM
	RET
END(fpsetmask)
