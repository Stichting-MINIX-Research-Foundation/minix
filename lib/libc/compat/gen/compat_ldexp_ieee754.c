/* $NetBSD: compat_ldexp_ieee754.c,v 1.5 2010/04/23 19:04:54 drochner Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
__RCSID("$NetBSD: compat_ldexp_ieee754.c,v 1.5 2010/04/23 19:04:54 drochner Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <machine/ieee.h>
#include <errno.h>

double ldexp(double, int);

/*
 * Multiply the given value by 2^expon.
 */
double
ldexp(double val, int expon)
{
	int oldexp, newexp;
	union ieee_double_u u, mul;

	u.dblu_d = val;
	oldexp = u.dblu_dbl.dbl_exp;

	/*
	 * If input is zero, Inf or NaN, just return it.
	 */
	if (u.dblu_d == 0.0 || oldexp == DBL_EXP_INFNAN)
		return (val);

	if (oldexp == 0) {
		/*
		 * u.v is denormal.  We must adjust it so that the exponent
		 * arithmetic below will work.
		 */
		if (expon <= DBL_EXP_BIAS) {
			/*
			 * Optimization: if the scaling can be done in a single
			 * multiply, or underflows, just do it now.
			 */
			if (expon <= -DBL_FRACBITS) {
				errno = ERANGE;
				return (val < 0.0 ? -0.0 : 0.0);
			}
			mul.dblu_d = 0.0;
			mul.dblu_dbl.dbl_exp = expon + DBL_EXP_BIAS;
			u.dblu_d *= mul.dblu_d;
			if (u.dblu_d == 0.0) {
				errno = ERANGE;
				return (val < 0.0 ? -0.0 : 0.0);
			}
			return (u.dblu_d);
		} else {
			/*
			 * We know that expon is very large, and therefore the
			 * result cannot be denormal (though it may be Inf).
			 * Shift u.v by just enough to make it normal.
			 */
			mul.dblu_d = 0.0;
			mul.dblu_dbl.dbl_exp = DBL_FRACBITS + DBL_EXP_BIAS;
			u.dblu_d *= mul.dblu_d;
			expon -= DBL_FRACBITS;
			oldexp = u.dblu_dbl.dbl_exp;
		}
	}

	/*
	 * u.v is now normalized and oldexp has been adjusted if necessary.
	 * Calculate the new exponent and check for underflow and overflow.
	 */
	newexp = oldexp + expon;

	if (newexp <= 0) {
		/*
		 * The output number is either denormal or underflows (see
		 * comments in machine/ieee.h).
		 */
		if (newexp <= -DBL_FRACBITS) {
			errno = ERANGE;
			return (val < 0.0 ? -0.0 : 0.0);
		}
		/*
		 * Denormalize the result.  We do this with a multiply.  If
		 * expon is very large, it won't fit in a double, so we have
		 * to adjust the exponent first.  This is safe because we know
		 * that u.v is normal at this point.
		 */
		if (expon <= -DBL_EXP_BIAS) {
			u.dblu_dbl.dbl_exp = 1;
			expon += oldexp - 1;
		}
		mul.dblu_d = 0.0;
		mul.dblu_dbl.dbl_exp = expon + DBL_EXP_BIAS;
		u.dblu_d *= mul.dblu_d;
		return (u.dblu_d);
	} else if (newexp >= DBL_EXP_INFNAN) {
		/*
		 * The result overflowed; return +/-Inf.
		 */
		u.dblu_dbl.dbl_exp = DBL_EXP_INFNAN;
		u.dblu_dbl.dbl_frach = 0;
		u.dblu_dbl.dbl_fracl = 0;
		errno = ERANGE;
		return (u.dblu_d);
	} else {
		/*
		 * The result is normal; just replace the old exponent with the
		 * new one.
		 */
		u.dblu_dbl.dbl_exp = newexp;
		return (u.dblu_d);
	}
}
