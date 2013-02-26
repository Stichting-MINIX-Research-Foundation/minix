/* $NetBSD: t_isnan.c,v 1.1 2011/09/19 05:25:50 jruoho Exp $ */

/*
 * This file is in the Public Domain.
 *
 * The nan test is blatently copied by Simon Burge from the infinity
 * test by Ben Harris.
 */

#include <atf-c.h>
#include <atf-c/config.h>

#include <math.h>
#include <string.h>

ATF_TC(isnan_basic);
ATF_TC_HEAD(isnan_basic, tc)
{
	atf_tc_set_md_var(tc, "descr", "Verify that isnan(3) works");
}

ATF_TC_BODY(isnan_basic, tc)
{
	/* NAN is meant to be a (float)NaN. */
	ATF_CHECK(isnan(NAN) != 0);
	ATF_CHECK(isnan((double)NAN) != 0);
}

ATF_TC(isinf_basic);
ATF_TC_HEAD(isinf_basic, tc)
{
	atf_tc_set_md_var(tc, "descr", "Verify that isinf(3) works");
}

ATF_TC_BODY(isinf_basic, tc)
{

	/* HUGE_VAL is meant to be an infinity. */
	ATF_CHECK(isinf(HUGE_VAL) != 0);

	/* HUGE_VALF is the float analog of HUGE_VAL. */
	ATF_CHECK(isinf(HUGE_VALF) != 0);

	/* HUGE_VALL is the long double analog of HUGE_VAL. */
	ATF_CHECK(isinf(HUGE_VALL) != 0);
}

ATF_TP_ADD_TCS(tp)
{
	const char *arch;

	arch = atf_config_get("atf_arch");

	if (strcmp("vax", arch) == 0 || strcmp("m68000", arch) == 0)
		atf_tc_skip("Test not applicable on %s", arch);
	else {
		ATF_TP_ADD_TC(tp, isnan_basic);
		ATF_TP_ADD_TC(tp, isinf_basic);
	}

	return atf_no_error();
}
