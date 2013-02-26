/* $NetBSD: t_atan.c,v 1.9 2013/06/14 05:39:28 isaki Exp $ */

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

static const struct {
	double x;
	double y;
} values[] = {
	{ -100, -1.560796660108231, },
	{  -10, -1.471127674303735, },
	{   -1, -M_PI / 4, },
	{ -0.1, -0.09966865249116204, },
	{  0.1,  0.09966865249116204, },
	{    1,  M_PI / 4, },
	{   10,  1.471127674303735, },
	{  100,  1.560796660108231, },
};

/*
 * atan(3)
 */
ATF_TC(atan_nan);
ATF_TC_HEAD(atan_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atan(NaN) == NaN");
}

ATF_TC_BODY(atan_nan, tc)
{
#ifndef __vax__
	const double x = 0.0L / 0.0L;

	if (isnan(atan(x)) == 0)
		atf_tc_fail_nonfatal("atan(NaN) != NaN");
#endif
}

ATF_TC(atan_inf_neg);
ATF_TC_HEAD(atan_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atan(-Inf) == -pi/2");
}

ATF_TC_BODY(atan_inf_neg, tc)
{
#ifndef __vax__
	const double x = -1.0L / 0.0L;
	const double eps = 1.0e-15;

	if (fabs(atan(x) + M_PI_2) > eps)
		atf_tc_fail_nonfatal("atan(-Inf) != -pi/2");
#endif
}

ATF_TC(atan_inf_pos);
ATF_TC_HEAD(atan_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atan(+Inf) == pi/2");
}

ATF_TC_BODY(atan_inf_pos, tc)
{
#ifndef __vax__
	const double x = +1.0L / 0.0L;
	const double eps = 1.0e-15;

	if (fabs(atan(x) - M_PI_2) > eps)
		atf_tc_fail_nonfatal("atan(+Inf) != pi/2");
#endif
}

ATF_TC(atan_inrange);
ATF_TC_HEAD(atan_inrange, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atan(x) for some values");
}

ATF_TC_BODY(atan_inrange, tc)
{
#ifndef __vax__
	const double eps = 1.0e-15;
	size_t i;

	for (i = 0; i < __arraycount(values); i++) {
		if (fabs(atan(values[i].x) - values[i].y) > eps)
			atf_tc_fail_nonfatal("atan(%g) != %g",
				values[i].x, values[i].y);
	}
#endif
}

ATF_TC(atan_zero_neg);
ATF_TC_HEAD(atan_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atan(-0.0) == -0.0");
}

ATF_TC_BODY(atan_zero_neg, tc)
{
#ifndef __vax__
	const double x = -0.0L;
	double y = atan(x);

	if (fabs(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("atan(-0.0) != -0.0");
#endif
}

ATF_TC(atan_zero_pos);
ATF_TC_HEAD(atan_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atan(+0.0) == +0.0");
}

ATF_TC_BODY(atan_zero_pos, tc)
{
#ifndef __vax__
	const double x = 0.0L;
	double y = atan(x);

	if (fabs(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("atan(+0.0) != +0.0");
#endif
}

/*
 * atanf(3)
 */
ATF_TC(atanf_nan);
ATF_TC_HEAD(atanf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atanf(NaN) == NaN");
}

ATF_TC_BODY(atanf_nan, tc)
{
#ifndef __vax__
	const float x = 0.0L / 0.0L;

	if (isnan(atanf(x)) == 0)
		atf_tc_fail_nonfatal("atanf(NaN) != NaN");
#endif
}

ATF_TC(atanf_inf_neg);
ATF_TC_HEAD(atanf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atanf(-Inf) == -pi/2");
}

ATF_TC_BODY(atanf_inf_neg, tc)
{
#ifndef __vax__
	const float x = -1.0L / 0.0L;
	const float eps = 1.0e-7;

	if (fabsf(atanf(x) + M_PI_2) > eps)
		atf_tc_fail_nonfatal("atanf(-Inf) != -pi/2");
#endif
}

ATF_TC(atanf_inf_pos);
ATF_TC_HEAD(atanf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atanf(+Inf) == pi/2");
}

ATF_TC_BODY(atanf_inf_pos, tc)
{
#ifndef __vax__
	const float x = +1.0L / 0.0L;
	const float eps = 1.0e-7;

	if (fabsf(atanf(x) - M_PI_2) > eps)
		atf_tc_fail_nonfatal("atanf(+Inf) != pi/2");
#endif
}

ATF_TC(atanf_inrange);
ATF_TC_HEAD(atanf_inrange, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atanf(x) for some values");
}

ATF_TC_BODY(atanf_inrange, tc)
{
#ifndef __vax__
	const float eps = 1.0e-7;
	float x;
	float y;
	size_t i;

	for (i = 0; i < __arraycount(values); i++) {
		x = values[i].x;
		y = values[i].y;
		if (fabs(atanf(x) - y) > eps)
			atf_tc_fail_nonfatal("atan(%g) != %g", x, y);
	}
#endif
}

ATF_TC(atanf_zero_neg);
ATF_TC_HEAD(atanf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atanf(-0.0) == -0.0");
}

ATF_TC_BODY(atanf_zero_neg, tc)
{
#ifndef __vax__
	const float x = -0.0L;
	float y = atanf(x);

	if (fabsf(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("atanf(-0.0) != -0.0");
#endif
}

ATF_TC(atanf_zero_pos);
ATF_TC_HEAD(atanf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test atanf(+0.0) == +0.0");
}

ATF_TC_BODY(atanf_zero_pos, tc)
{
#ifndef __vax__
	const float x = 0.0L;
	float y = atanf(x);

	if (fabsf(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("atanf(+0.0) != +0.0");
#endif
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, atan_nan);
	ATF_TP_ADD_TC(tp, atan_inf_neg);
	ATF_TP_ADD_TC(tp, atan_inf_pos);
	ATF_TP_ADD_TC(tp, atan_inrange);
	ATF_TP_ADD_TC(tp, atan_zero_neg);
	ATF_TP_ADD_TC(tp, atan_zero_pos);

	ATF_TP_ADD_TC(tp, atanf_nan);
	ATF_TP_ADD_TC(tp, atanf_inf_neg);
	ATF_TP_ADD_TC(tp, atanf_inf_pos);
	ATF_TP_ADD_TC(tp, atanf_inrange);
	ATF_TP_ADD_TC(tp, atanf_zero_neg);
	ATF_TP_ADD_TC(tp, atanf_zero_pos);

	return atf_no_error();
}
