/*	$NetBSD: t_copy.c,v 1.1 2010/11/09 15:25:20 pooka Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>

#include <atf-c.h>
#include <errno.h>
#include <string.h>

#include <rump/rump.h>

ATF_TC(copystr);
ATF_TC_HEAD(copystr, tc)
{

	atf_tc_set_md_var(tc, "descr", "Tests copystr()");
}

ATF_TC(copyinstr);
ATF_TC_HEAD(copyinstr, tc)
{

	atf_tc_set_md_var(tc, "descr", "Tests copyinstr()");
}

ATF_TC(copyoutstr);
ATF_TC_HEAD(copyoutstr, tc)
{

	atf_tc_set_md_var(tc, "descr", "Tests copyoutstr()");
}

typedef int (copy_fn)(const void *, void *, size_t, size_t *);

extern copy_fn rumpns_copystr, rumpns_copyinstr, rumpns_copyoutstr;

#define TESTSTR "jippii, lisaa puuroa"

static void
dotest(copy_fn *thefun)
{
	char buf[sizeof(TESTSTR)+1];
	size_t len;

	rump_init();
	rump_schedule();

	/* larger buffer */
	memset(buf, 0xaa, sizeof(buf));
	ATF_REQUIRE_EQ(thefun(TESTSTR, buf, sizeof(buf), &len), 0);
	ATF_REQUIRE_EQ(len, sizeof(TESTSTR));
	ATF_REQUIRE_STREQ(TESTSTR, buf);

	/* just large enough */
	memset(buf, 0xaa, sizeof(buf));
	ATF_REQUIRE_EQ(thefun(TESTSTR, buf, sizeof(buf)-1, &len), 0);
	ATF_REQUIRE_EQ(len, sizeof(TESTSTR));
	ATF_REQUIRE_STREQ(TESTSTR, buf);

	/* one too small */
	memset(buf, 0xaa, sizeof(buf));
	ATF_REQUIRE_EQ(thefun(TESTSTR, buf, sizeof(buf)-2, NULL), ENAMETOOLONG);

	rump_unschedule();
}

ATF_TC_BODY(copystr, tc)
{

	dotest(rumpns_copystr);
}

ATF_TC_BODY(copyinstr, tc)
{

	dotest(rumpns_copyinstr);
}

ATF_TC_BODY(copyoutstr, tc)
{

	dotest(rumpns_copyoutstr);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, copystr);
	ATF_TP_ADD_TC(tp, copyinstr);
	ATF_TP_ADD_TC(tp, copyoutstr);

	return atf_no_error();
}
