/*	$NetBSD: fpsetround.c,v 1.8 2013/02/03 01:50:54 matt Exp $	*/

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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetround.c,v 1.8 2013/02/03 01:50:54 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <ieeefp.h>

#include <arm/vfpreg.h>

#ifdef __weak_alias
__weak_alias(fpsetround,_fpsetround)
#endif

/*
 * Return the current FP rounding mode
 */

fp_rnd
fpsetround(fp_rnd new_rnd)
{
	__CTASSERT(__SHIFTOUT(VFP_FPSCR_RN, VFP_FPSCR_RMODE) == FP_RN);
	__CTASSERT(__SHIFTOUT(VFP_FPSCR_RP, VFP_FPSCR_RMODE) == FP_RP);
	__CTASSERT(__SHIFTOUT(VFP_FPSCR_RM, VFP_FPSCR_RMODE) == FP_RM);
	__CTASSERT(__SHIFTOUT(VFP_FPSCR_RZ, VFP_FPSCR_RMODE) == FP_RZ);
	uint32_t fpscr; 
	__asm __volatile("vmrs %0, fpscr" : "=r" (fpscr));
	fp_rnd old_rnd = __SHIFTOUT(fpscr, VFP_FPSCR_RMODE);
	fpscr ^= __SHIFTIN(new_rnd ^ old_rnd, VFP_FPSCR_RMODE);
	__asm __volatile("vmsr fpscr, %0" :: "r" (fpscr));
	return old_rnd;
}
