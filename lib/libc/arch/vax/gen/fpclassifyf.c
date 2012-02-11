/*	$NetBSD: fpclassifyf.c,v 1.3 2008/04/28 20:22:57 martin Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpclassifyf.c,v 1.3 2008/04/28 20:22:57 martin Exp $");
#endif

#include <machine/vaxfp.h>
#include <math.h>

/*
 * 7.12.3.1 fpclassify - classify real floating type
 *          VAX F_floating version
 *
 *          Implementation notes:
 *          Reserved operand -> FP_ROP
 *          True zero        -> FP_ZERO
 *          Dirty zero       -> FP_DIRTY_ZERO
 *          Finite           -> FP_NORMAL
 */
int
__fpclassifyf(float x)
{
	union vax_ffloating_u u;

	u.ffltu_f = x;

	if (u.ffltu_fflt.fflt_exp == 0) {
		if (u.ffltu_fflt.fflt_sign != 0)
			return FP_ROP;
		else if (u.ffltu_fflt.fflt_frach != 0 ||
			 u.ffltu_fflt.fflt_fracl != 0) 
			return FP_DIRTYZERO;
		else
			return FP_ZERO;
	} else {
		return FP_NORMAL;
	}
}
