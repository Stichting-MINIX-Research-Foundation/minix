/*	$NetBSD: fpsetsticky.c,v 1.11 2011/07/10 21:18:47 matt Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 * 
 * This code is derived from software contributed to The NetBSD Foundation
 * by Dan Winship.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetsticky.c,v 1.11 2011/07/10 21:18:47 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <ieeefp.h>
#include <powerpc/fpu.h>

#define	STICKYBITS	(FPSCR_XX|FPSCR_ZX|FPSCR_UX|FPSCR_OX|FPSCR_VX)
#define	INVBITS		(FPSCR_VXCVI|FPSCR_VXSQRT|FPSCR_VXSOFT|FPSCR_VXVC|\
			 FPSCR_VXIMZ|FPSCR_VXZDZ|FPSCR_VXIDI|FPSCR_VXISI|\
			 FPSCR_VXSNAN)
#define	STICKYSHFT	25

#ifdef __weak_alias
__weak_alias(fpsetsticky,_fpsetsticky)
#endif

fp_except
fpsetsticky(fp_except mask)
{
	union {
		double u_d;
		uint64_t u_fpscr;
	} ud;
	fp_except old;

	__asm volatile("mffs %0" : "=f"(ud.u_d));
	old = ((uint32_t)ud.u_fpscr & STICKYBITS) >> STICKYSHFT;
	/*
	 * FPSCR_VX (aka FP_X_INV) is not a sticky bit but a summary of the
	 * all the FPSCR_VX* sticky bits.  So when FP_X_INV is cleared then
	 * clear all of those bits, likewise when it's set, set them all.
	 */
	if ((mask & FP_X_INV) == 0)
		ud.u_fpscr &= ~INVBITS;
	else 
		ud.u_fpscr |= INVBITS;
	ud.u_fpscr &= ~STICKYBITS;
	ud.u_fpscr |= ((uint32_t)mask << STICKYSHFT) & STICKYBITS;
	/*
	 * Make FPSCR_FX reflect the presence of a set sticky bit (or not).
	 */
	if (ud.u_fpscr & (STICKYBITS|INVBITS))
		ud.u_fpscr |= FPSCR_FX;
	else
		ud.u_fpscr &= ~FPSCR_FX;
	/*
	 * Write back the fpscr.
	 */
	__asm volatile("mtfsf 0xff,%0" :: "f"(ud.u_d));
	return (old);
}
