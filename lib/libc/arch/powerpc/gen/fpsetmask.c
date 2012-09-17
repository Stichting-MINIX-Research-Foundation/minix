/*	$NetBSD: fpsetmask.c,v 1.10 2011/07/10 21:18:47 matt Exp $	*/

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
__RCSID("$NetBSD: fpsetmask.c,v 1.10 2011/07/10 21:18:47 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <ieeefp.h>
#include <powerpc/fpu.h>

#ifdef __weak_alias
__weak_alias(fpsetmask,_fpsetmask)
#endif

#define	MASKBITS	(FPSCR_XE|FPSCR_ZE|FPSCR_UE|FPSCR_OE|FPSCR_VE)
#define	MASKSHFT	3

fp_except
fpsetmask(fp_except mask)
{
	union {
		double u_d;
		uint64_t u_fpscr;
	} ud;
	fp_except old;

	__asm volatile("mffs %0" : "=f"(ud.u_d));
	old = ((uint32_t)ud.u_fpscr & MASKBITS) >> MASKSHFT;
	ud.u_fpscr &= ~MASKBITS;
	ud.u_fpscr |= ((uint32_t)mask << MASKSHFT) & MASKBITS;
	__asm volatile("mtfsf 0xff,%0" :: "f"(ud.u_d));
	return (old);
}
