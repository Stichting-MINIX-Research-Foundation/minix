/*	$NetBSD: s_ilogbl.c,v 1.2 2013/02/09 22:56:00 matt Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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
__RCSID("$NetBSD: s_ilogbl.c,v 1.2 2013/02/09 22:56:00 matt Exp $");

#include "namespace.h"

#include <float.h>
#include <math.h>
#include <machine/ieee.h>

#ifdef __HAVE_LONG_DOUBLE

#if LDBL_MANT_DIG == 64
#define	FROM_UNDERFLOW	0x1p65L
#elif LDBL_MANT_DIG == 113
#define	FROM_UNDERFLOW	0x1p114L
#else
#error Unsupported long double format
#endif

int
ilogbl(long double x)
{
	union ieee_ext_u u;

	if (x == 0.0L)
		return FP_ILOGB0;	/* ilogbl(0) = 0x80000001 */

	u.extu_ld = x;

	if (u.extu_ext.ext_exp == EXT_EXP_INFNAN)
		return FP_ILOGBNAN;	/* inf too */

	if (u.extu_ext.ext_exp == 0) {
		/*
		 * Scale denormalized numbers slightly,
		 * so that they are normal.
		 */
		u.extu_ld *= FROM_UNDERFLOW;
		return u.extu_ext.ext_exp - EXT_EXP_BIAS - LDBL_MANT_DIG - 1;
	}
	return u.extu_ext.ext_exp - EXT_EXP_BIAS;

}

#endif
