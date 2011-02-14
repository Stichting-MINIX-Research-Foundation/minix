/*	$NetBSD: fpclassifyl_ieee754.c,v 1.1 2011/01/17 23:53:03 matt Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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
__RCSID("$NetBSD: fpclassifyl_ieee754.c,v 1.1 2011/01/17 23:53:03 matt Exp $");
#endif

#include <machine/ieee.h>
#include <math.h>

#ifdef __HAVE_LONG_DOUBLE
/*
 * 7.12.3.1 fpclassify - classify real floating type
 *          IEEE 754 compatible 128-bit extended-precision version
 */
int
__fpclassifyl(long double x)
{
	union ieee_ext_u u;

	u.extu_ld = x;

	if (u.extu_ext.ext_exp == 0) {
		if (u.extu_ext.ext_frach == 0 && u.extu_ext.ext_frachm == 0
		    && u.extu_ext.ext_fraclm == 0 && u.extu_ext.ext_fracl == 0)
			return FP_ZERO;
		else
			return FP_SUBNORMAL;
	} else if (u.extu_ext.ext_exp == EXT_EXP_INFNAN) {
		if (u.extu_ext.ext_frach == 0 && u.extu_ext.ext_frachm == 0
		    && u.extu_ext.ext_fraclm == 0 && u.extu_ext.ext_fracl == 0)
			return FP_INFINITE;
		else
			return FP_NAN;
	}

	return FP_NORMAL;
}
#endif /* __HAVE_LONG_DOUBLE */
