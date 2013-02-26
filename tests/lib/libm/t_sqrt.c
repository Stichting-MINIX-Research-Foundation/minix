/* $NetBSD: t_sqrt.c,v 1.3 2012/02/13 05:09:01 jruoho Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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
__RCSID("$NetBSD: t_sqrt.c,v 1.3 2012/02/13 05:09:01 jruoho Exp $");

#include <atf-c.h>
#include <math.h>
#include <stdio.h>

/*
 * sqrt(3)
 */
ATF_TC(sqrt_nan);
ATF_TC_HEAD(sqrt_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrt(NaN) == NaN");
}

ATF_TC_BODY(sqrt_nan, tc)
{
#ifndef __vax__
	const double x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(sqrt(x)) != 0);
#endif
}

ATF_TC(sqrt_pow);
ATF_TC_HEAD(sqrt_pow, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrt(3) vs. pow(3)");
}

ATF_TC_BODY(sqrt_pow, tc)
{
#ifndef __vax__
	const double x[] = { 0.0, 0.005, 1.0, 99.0, 123.123, 9999.9999 };
	const double eps = 1.0e-40;
	double y, z;
	size_t i;

	for (i = 0; i < __arraycount(x); i++) {

		y = sqrt(x[i]);
		z = pow(x[i], 1.0 / 2.0);

		if (fabs(y - z) > eps)
			atf_tc_fail_nonfatal("sqrt(%0.03f) != "
			    "pow(%0.03f, 1/2)\n", x[i], x[i]);
	}
#endif
}

ATF_TC(sqrt_inf_neg);
ATF_TC_HEAD(sqrt_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrt(-Inf) == NaN");
}

ATF_TC_BODY(sqrt_inf_neg, tc)
{
#ifndef __vax__
	const double x = -1.0L / 0.0L;
	double y = sqrt(x);

	ATF_CHECK(isnan(y) != 0);
#endif
}

ATF_TC(sqrt_inf_pos);
ATF_TC_HEAD(sqrt_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrt(+Inf) == +Inf");
}

ATF_TC_BODY(sqrt_inf_pos, tc)
{
#ifndef __vax__
	const double x = 1.0L / 0.0L;
	double y = sqrt(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) == 0);
#endif
}

ATF_TC(sqrt_zero_neg);
ATF_TC_HEAD(sqrt_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrt(-0.0) == -0.0");
}

ATF_TC_BODY(sqrt_zero_neg, tc)
{
#ifndef __vax__
	const double x = -0.0L;
	double y = sqrt(x);

	if (fabs(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("sqrt(-0.0) != -0.0");
#endif
}

ATF_TC(sqrt_zero_pos);
ATF_TC_HEAD(sqrt_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrt(+0.0) == +0.0");
}

ATF_TC_BODY(sqrt_zero_pos, tc)
{
#ifndef __vax__
	const double x = 0.0L;
	double y = sqrt(x);

	if (fabs(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("sqrt(+0.0) != +0.0");
#endif
}

/*
 * sqrtf(3)
 */
ATF_TC(sqrtf_nan);
ATF_TC_HEAD(sqrtf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrtf(NaN) == NaN");
}

ATF_TC_BODY(sqrtf_nan, tc)
{
#ifndef __vax__
	const float x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(sqrtf(x)) != 0);
#endif
}

ATF_TC(sqrtf_powf);
ATF_TC_HEAD(sqrtf_powf, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrtf(3) vs. powf(3)");
}

ATF_TC_BODY(sqrtf_powf, tc)
{
#ifndef __vax__
	const float x[] = { 0.0, 0.005, 1.0, 99.0, 123.123, 9999.9999 };
	const float eps = 1.0e-30;
	volatile float y, z;
	size_t i;

	for (i = 0; i < __arraycount(x); i++) {

		y = sqrtf(x[i]);
		z = powf(x[i], 1.0 / 2.0);

		if (fabsf(y - z) > eps)
			atf_tc_fail_nonfatal("sqrtf(%0.03f) != "
			    "powf(%0.03f, 1/2)\n", x[i], x[i]);
	}
#endif
}

ATF_TC(sqrtf_inf_neg);
ATF_TC_HEAD(sqrtf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrtf(-Inf) == NaN");
}

ATF_TC_BODY(sqrtf_inf_neg, tc)
{
#ifndef __vax__
	const float x = -1.0L / 0.0L;
	float y = sqrtf(x);

	ATF_CHECK(isnan(y) != 0);
#endif
}

ATF_TC(sqrtf_inf_pos);
ATF_TC_HEAD(sqrtf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrtf(+Inf) == +Inf");
}

ATF_TC_BODY(sqrtf_inf_pos, tc)
{
#ifndef __vax__
	const float x = 1.0L / 0.0L;
	float y = sqrtf(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) == 0);
#endif
}

ATF_TC(sqrtf_zero_neg);
ATF_TC_HEAD(sqrtf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrtf(-0.0) == -0.0");
}

ATF_TC_BODY(sqrtf_zero_neg, tc)
{
#ifndef __vax__
	const float x = -0.0L;
	float y = sqrtf(x);

	if (fabsf(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("sqrtf(-0.0) != -0.0");
#endif
}

ATF_TC(sqrtf_zero_pos);
ATF_TC_HEAD(sqrtf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sqrtf(+0.0) == +0.0");
}

ATF_TC_BODY(sqrtf_zero_pos, tc)
{
#ifndef __vax__
	const float x = 0.0L;
	float y = sqrtf(x);

	if (fabsf(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("sqrtf(+0.0) != +0.0");
#endif
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, sqrt_nan);
	ATF_TP_ADD_TC(tp, sqrt_pow);
	ATF_TP_ADD_TC(tp, sqrt_inf_neg);
	ATF_TP_ADD_TC(tp, sqrt_inf_pos);
	ATF_TP_ADD_TC(tp, sqrt_zero_neg);
	ATF_TP_ADD_TC(tp, sqrt_zero_pos);

	ATF_TP_ADD_TC(tp, sqrtf_nan);
	ATF_TP_ADD_TC(tp, sqrtf_powf);
	ATF_TP_ADD_TC(tp, sqrtf_inf_neg);
	ATF_TP_ADD_TC(tp, sqrtf_inf_pos);
	ATF_TP_ADD_TC(tp, sqrtf_zero_neg);
	ATF_TP_ADD_TC(tp, sqrtf_zero_pos);

	return atf_no_error();
}
