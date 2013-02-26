/* $NetBSD: t_exp.c,v 1.3 2013/04/09 11:42:56 isaki Exp $ */

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

#include <atf-c.h>
#include <math.h>

/* y = exp(x) */
static const struct {
	double x;
	double y;
	double e;
} exp_values[] = {
	{  -10, 0.4539992976248485e-4, 1e-4, },
	{   -5, 0.6737946999085467e-2, 1e-2, },
	{   -1, 0.3678794411714423,    1e-1, },
	{ -0.1, 0.9048374180359595,    1e-1, },
	{    0, 1.0000000000000000,    1,    },
	{  0.1, 1.1051709180756477,    1,    },
	{    1, 2.7182818284590452,    1,    },
	{    5, 148.41315910257660,    1e2, },
	{   10, 22026.465794806718,    1e4, },
};

/*
 * exp2(3)
 */
ATF_TC(exp2_nan);
ATF_TC_HEAD(exp2_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2(NaN) == NaN");
}

ATF_TC_BODY(exp2_nan, tc)
{
#ifndef __vax__
	const double x = 0.0L / 0.0L;

	if (isnan(exp2(x)) == 0)
		atf_tc_fail_nonfatal("exp2(NaN) != NaN");
#endif
}

ATF_TC(exp2_inf_neg);
ATF_TC_HEAD(exp2_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2(-Inf) == +0.0");
}

ATF_TC_BODY(exp2_inf_neg, tc)
{
#ifndef __vax__
	const double x = -1.0L / 0.0L;
	double y = exp2(x);

	if (fabs(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("exp2(-Inf) != +0.0");
#endif
}

ATF_TC(exp2_inf_pos);
ATF_TC_HEAD(exp2_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2(+Inf) == +Inf");
}

ATF_TC_BODY(exp2_inf_pos, tc)
{
#ifndef __vax__
	const double x = 1.0L / 0.0L;
	double y = exp2(x);

	if (isinf(y) == 0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("exp2(+Inf) != +Inf");
#endif
}

ATF_TC(exp2_product);
ATF_TC_HEAD(exp2_product, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2(x + y) == exp2(x) * exp2(y)");
}

ATF_TC_BODY(exp2_product, tc)
{
#ifndef __vax__
	const double x[] = { 0.0, 1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8 };
	const double y[] = { 8.8, 7.7, 6.6, 5.5, 4.4, 3.3, 2.2, 1.1, 0.0 };
	const double eps = 1.0e-11;
	size_t i;

	for (i = 0; i < __arraycount(x); i++) {

		if (fabs(exp2(x[i] + y[i]) - (exp2(x[i]) * exp2(y[i]))) > eps)
			atf_tc_fail_nonfatal("exp2(%0.01f + %0.01f) != exp2("
			    "%0.01f) * exp2(%0.01f)", x[i], y[i], x[i], y[i]);
	}
#endif
}

ATF_TC(exp2_zero_neg);
ATF_TC_HEAD(exp2_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2(-0.0) == 1.0");
}

ATF_TC_BODY(exp2_zero_neg, tc)
{
#ifndef __vax__
	const double x = -0.0L;

	if (fabs(exp2(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("exp2(-0.0) != 1.0");
#endif
}

ATF_TC(exp2_zero_pos);
ATF_TC_HEAD(exp2_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2(+0.0) == 1.0");
}

ATF_TC_BODY(exp2_zero_pos, tc)
{
#ifndef __vax__
	const double x = 0.0L;

	if (fabs(exp2(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("exp2(+0.0) != 1.0");
#endif
}

/*
 * exp2f(3)
 */
ATF_TC(exp2f_nan);
ATF_TC_HEAD(exp2f_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2f(NaN) == NaN");
}

ATF_TC_BODY(exp2f_nan, tc)
{
#ifndef __vax__
	const float x = 0.0L / 0.0L;

	if (isnan(exp2f(x)) == 0)
		atf_tc_fail_nonfatal("exp2f(NaN) != NaN");
#endif
}

ATF_TC(exp2f_inf_neg);
ATF_TC_HEAD(exp2f_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2f(-Inf) == +0.0");
}

ATF_TC_BODY(exp2f_inf_neg, tc)
{
#ifndef __vax__
	const float x = -1.0L / 0.0L;
	float y = exp2f(x);

	if (fabsf(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("exp2f(-Inf) != +0.0");
#endif
}

ATF_TC(exp2f_inf_pos);
ATF_TC_HEAD(exp2f_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2f(+Inf) == +Inf");
}

ATF_TC_BODY(exp2f_inf_pos, tc)
{
#ifndef __vax__
	const float x = 1.0L / 0.0L;
	float y = exp2f(x);

	if (isinf(y) == 0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("exp2f(+Inf) != +Inf");
#endif
}

ATF_TC(exp2f_product);
ATF_TC_HEAD(exp2f_product, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2f(x+y) == exp2f(x) * exp2f(y)");
}

ATF_TC_BODY(exp2f_product, tc)
{
#ifndef __vax__
	const float x[] = { 0.0, 1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8 };
	const float y[] = { 8.8, 7.7, 6.6, 5.5, 4.4, 3.3, 2.2, 1.1, 0.0 };
	const float eps = 1.0e-2;
	size_t i;

	for (i = 0; i < __arraycount(x); i++) {

		if (fabsf(exp2f(x[i] + y[i]) -
			(exp2f(x[i]) * exp2f(y[i]))) > eps)
			atf_tc_fail_nonfatal("exp2f(%0.01f + %0.01f) != exp2f("
			    "%0.01f) * exp2f(%0.01f)", x[i], y[i], x[i], y[i]);
	}
#endif
}

ATF_TC(exp2f_zero_neg);
ATF_TC_HEAD(exp2f_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2f(-0.0) == 1.0");
}

ATF_TC_BODY(exp2f_zero_neg, tc)
{
#ifndef __vax__
	const float x = -0.0L;

	if (fabsf(exp2f(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("exp2f(-0.0) != 1.0");
#endif
}

ATF_TC(exp2f_zero_pos);
ATF_TC_HEAD(exp2f_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp2f(+0.0) == 1.0");
}

ATF_TC_BODY(exp2f_zero_pos, tc)
{
#ifndef __vax__
	const float x = 0.0L;

	if (fabsf(exp2f(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("exp2f(+0.0) != 1.0");
#endif
}

/*
 * exp(3)
 */
ATF_TC(exp_nan);
ATF_TC_HEAD(exp_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp(NaN) == NaN");
}

ATF_TC_BODY(exp_nan, tc)
{
#ifndef __vax__
	const double x = 0.0L / 0.0L;

	if (isnan(exp(x)) == 0)
		atf_tc_fail_nonfatal("exp(NaN) != NaN");
#endif
}

ATF_TC(exp_inf_neg);
ATF_TC_HEAD(exp_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp(-Inf) == +0.0");
}

ATF_TC_BODY(exp_inf_neg, tc)
{
#ifndef __vax__
	const double x = -1.0L / 0.0L;
	double y = exp(x);

	if (fabs(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("exp(-Inf) != +0.0");
#endif
}

ATF_TC(exp_inf_pos);
ATF_TC_HEAD(exp_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp(+Inf) == +Inf");
}

ATF_TC_BODY(exp_inf_pos, tc)
{
#ifndef __vax__
	const double x = 1.0L / 0.0L;
	double y = exp(x);

	if (isinf(y) == 0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("exp(+Inf) != +Inf");
#endif
}

ATF_TC(exp_product);
ATF_TC_HEAD(exp_product, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected exp(x)");
}

ATF_TC_BODY(exp_product, tc)
{
#ifndef __vax__
	double eps;
	double x;
	double y;
	size_t i;

	for (i = 0; i < __arraycount(exp_values); i++) {
		x = exp_values[i].x;
		y = exp_values[i].y;
		eps = 1e-15 * exp_values[i].e;

		if (fabs(exp(x) - y) > eps)
			atf_tc_fail_nonfatal("exp(%0.01f) != %18.18e", x, y);
	}
#endif
}

ATF_TC(exp_zero_neg);
ATF_TC_HEAD(exp_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp(-0.0) == 1.0");
}

ATF_TC_BODY(exp_zero_neg, tc)
{
#ifndef __vax__
	const double x = -0.0L;

	if (fabs(exp(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("exp(-0.0) != 1.0");
#endif
}

ATF_TC(exp_zero_pos);
ATF_TC_HEAD(exp_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test exp(+0.0) == 1.0");
}

ATF_TC_BODY(exp_zero_pos, tc)
{
#ifndef __vax__
	const double x = 0.0L;

	if (fabs(exp(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("exp(+0.0) != 1.0");
#endif
}

/*
 * expf(3)
 */
ATF_TC(expf_nan);
ATF_TC_HEAD(expf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expf(NaN) == NaN");
}

ATF_TC_BODY(expf_nan, tc)
{
#ifndef __vax__
	const float x = 0.0L / 0.0L;

	if (isnan(expf(x)) == 0)
		atf_tc_fail_nonfatal("expf(NaN) != NaN");
#endif
}

ATF_TC(expf_inf_neg);
ATF_TC_HEAD(expf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expf(-Inf) == +0.0");
}

ATF_TC_BODY(expf_inf_neg, tc)
{
#ifndef __vax__
	const float x = -1.0L / 0.0L;
	float y = expf(x);

	if (fabsf(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("expf(-Inf) != +0.0");
#endif
}

ATF_TC(expf_inf_pos);
ATF_TC_HEAD(expf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expf(+Inf) == +Inf");
}

ATF_TC_BODY(expf_inf_pos, tc)
{
#ifndef __vax__
	const float x = 1.0L / 0.0L;
	float y = expf(x);

	if (isinf(y) == 0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("expf(+Inf) != +Inf");
#endif
}

ATF_TC(expf_product);
ATF_TC_HEAD(expf_product, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected expf(x)");
}

ATF_TC_BODY(expf_product, tc)
{
#ifndef __vax__
	float eps;
	float x;
	float y;
	size_t i;

	for (i = 0; i < __arraycount(exp_values); i++) {
		x = exp_values[i].x;
		y = exp_values[i].y;
		eps = 1e-6 * exp_values[i].e;

		if (fabsf(expf(x) - y) > eps)
			atf_tc_fail_nonfatal("expf(%0.01f) != %18.18e", x, y);
	}
#endif
}

ATF_TC(expf_zero_neg);
ATF_TC_HEAD(expf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expf(-0.0) == 1.0");
}

ATF_TC_BODY(expf_zero_neg, tc)
{
#ifndef __vax__
	const float x = -0.0L;

	if (fabsf(expf(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("expf(-0.0) != 1.0");
#endif
}

ATF_TC(expf_zero_pos);
ATF_TC_HEAD(expf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expf(+0.0) == 1.0");
}

ATF_TC_BODY(expf_zero_pos, tc)
{
#ifndef __vax__
	const float x = 0.0L;

	if (fabsf(expf(x) - 1.0) > 0.0)
		atf_tc_fail_nonfatal("expf(+0.0) != 1.0");
#endif
}

/*
 * expm1(3)
 */
ATF_TC(expm1_nan);
ATF_TC_HEAD(expm1_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1(NaN) == NaN");
}

ATF_TC_BODY(expm1_nan, tc)
{
#ifndef __vax__
	const double x = 0.0L / 0.0L;

	if (isnan(expm1(x)) == 0)
		atf_tc_fail_nonfatal("expm1(NaN) != NaN");
#endif
}

ATF_TC(expm1_inf_neg);
ATF_TC_HEAD(expm1_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1(-Inf) == -1");
}

ATF_TC_BODY(expm1_inf_neg, tc)
{
#ifndef __vax__
	const double x = -1.0L / 0.0L;

	if (expm1(x) != -1.0)
		atf_tc_fail_nonfatal("expm1(-Inf) != -1.0");
#endif
}

ATF_TC(expm1_inf_pos);
ATF_TC_HEAD(expm1_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1(+Inf) == +Inf");
}

ATF_TC_BODY(expm1_inf_pos, tc)
{
#ifndef __vax__
	const double x = 1.0L / 0.0L;
	double y = expm1(x);

	if (isinf(y) == 0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("expm1(+Inf) != +Inf");
#endif
}

ATF_TC(expm1_zero_neg);
ATF_TC_HEAD(expm1_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1(-0.0) == -0.0");
}

ATF_TC_BODY(expm1_zero_neg, tc)
{
#ifndef __vax__
	const double x = -0.0L;
	double y = expm1(x);

	if (fabs(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("expm1(-0.0) != -0.0");
#endif
}

ATF_TC(expm1_zero_pos);
ATF_TC_HEAD(expm1_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1(+0.0) == 1.0");
}

ATF_TC_BODY(expm1_zero_pos, tc)
{
#ifndef __vax__
	const double x = 0.0L;
	double y = expm1(x);

	if (fabs(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("expm1(+0.0) != +0.0");
#endif
}

/*
 * expm1f(3)
 */
ATF_TC(expm1f_nan);
ATF_TC_HEAD(expm1f_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1f(NaN) == NaN");
}

ATF_TC_BODY(expm1f_nan, tc)
{
#ifndef __vax__
	const float x = 0.0L / 0.0L;

	if (isnan(expm1f(x)) == 0)
		atf_tc_fail_nonfatal("expm1f(NaN) != NaN");
#endif
}

ATF_TC(expm1f_inf_neg);
ATF_TC_HEAD(expm1f_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1f(-Inf) == -1");
}

ATF_TC_BODY(expm1f_inf_neg, tc)
{
#ifndef __vax__
	const float x = -1.0L / 0.0L;

	if (expm1f(x) != -1.0)
		atf_tc_fail_nonfatal("expm1f(-Inf) != -1.0");
#endif
}

ATF_TC(expm1f_inf_pos);
ATF_TC_HEAD(expm1f_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1f(+Inf) == +Inf");
}

ATF_TC_BODY(expm1f_inf_pos, tc)
{
#ifndef __vax__
	const float x = 1.0L / 0.0L;
	float y = expm1f(x);

	if (isinf(y) == 0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("expm1f(+Inf) != +Inf");
#endif
}

ATF_TC(expm1f_zero_neg);
ATF_TC_HEAD(expm1f_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1f(-0.0) == -0.0");
}

ATF_TC_BODY(expm1f_zero_neg, tc)
{
#ifndef __vax__
	const float x = -0.0L;
	float y = expm1f(x);

	if (fabsf(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("expm1f(-0.0) != -0.0");
#endif
}

ATF_TC(expm1f_zero_pos);
ATF_TC_HEAD(expm1f_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test expm1f(+0.0) == 1.0");
}

ATF_TC_BODY(expm1f_zero_pos, tc)
{
#ifndef __vax__
	const float x = 0.0L;
	float y = expm1f(x);

	if (fabsf(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("expm1f(+0.0) != +0.0");
#endif
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, exp2_nan);
	ATF_TP_ADD_TC(tp, exp2_inf_neg);
	ATF_TP_ADD_TC(tp, exp2_inf_pos);
	ATF_TP_ADD_TC(tp, exp2_product);
	ATF_TP_ADD_TC(tp, exp2_zero_neg);
	ATF_TP_ADD_TC(tp, exp2_zero_pos);

	ATF_TP_ADD_TC(tp, exp2f_nan);
	ATF_TP_ADD_TC(tp, exp2f_inf_neg);
	ATF_TP_ADD_TC(tp, exp2f_inf_pos);
	ATF_TP_ADD_TC(tp, exp2f_product);
	ATF_TP_ADD_TC(tp, exp2f_zero_neg);
	ATF_TP_ADD_TC(tp, exp2f_zero_pos);

	ATF_TP_ADD_TC(tp, exp_nan);
	ATF_TP_ADD_TC(tp, exp_inf_neg);
	ATF_TP_ADD_TC(tp, exp_inf_pos);
	ATF_TP_ADD_TC(tp, exp_product);
	ATF_TP_ADD_TC(tp, exp_zero_neg);
	ATF_TP_ADD_TC(tp, exp_zero_pos);

	ATF_TP_ADD_TC(tp, expf_nan);
	ATF_TP_ADD_TC(tp, expf_inf_neg);
	ATF_TP_ADD_TC(tp, expf_inf_pos);
	ATF_TP_ADD_TC(tp, expf_product);
	ATF_TP_ADD_TC(tp, expf_zero_neg);
	ATF_TP_ADD_TC(tp, expf_zero_pos);

	ATF_TP_ADD_TC(tp, expm1_nan);
	ATF_TP_ADD_TC(tp, expm1_inf_neg);
	ATF_TP_ADD_TC(tp, expm1_inf_pos);
	ATF_TP_ADD_TC(tp, expm1_zero_neg);
	ATF_TP_ADD_TC(tp, expm1_zero_pos);

	ATF_TP_ADD_TC(tp, expm1f_nan);
	ATF_TP_ADD_TC(tp, expm1f_inf_neg);
	ATF_TP_ADD_TC(tp, expm1f_inf_pos);
	ATF_TP_ADD_TC(tp, expm1f_zero_neg);
	ATF_TP_ADD_TC(tp, expm1f_zero_pos);

	return atf_no_error();
}
