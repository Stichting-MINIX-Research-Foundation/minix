/* $NetBSD: t_parser.c,v 1.4 2011/02/12 23:21:33 christos Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  	  This product includes software developed by the NetBSD
 *  	  Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_parser.c,v 1.4 2011/02/12 23:21:33 christos Exp $");

#include <atf-c.h>
#include <saslc.h>
#include <stdio.h>

#include "dict.h"
#include "parser.h"


/*
 * XXX: SASLC_TEST_DIR must be set to the current dir in order to pick up
 * the config files.  It is currently set in the Makefile.
 */

/* src/parser.c test cases */

static void set_env(atf_tc_t *tc)
{
        char *dir;

        asprintf(&dir, "%s/%s/", atf_tc_get_config_var(tc, "srcdir"),
            "parser_tests");

        if (dir == NULL)
            exit(-1);

        setenv(SASLC_ENV_CONFIG, dir, 1);
        free(dir);
}


ATF_TC(t_parser_test1);
ATF_TC_HEAD(t_parser_test1, tc)
{
	set_env(tc);
	atf_tc_set_md_var(tc, "descr", "parser test1");
}
ATF_TC_BODY(t_parser_test1, tc)
{
	saslc_t *ctx;

	ATF_REQUIRE(ctx = saslc_alloc());
	ATF_CHECK_EQ(saslc_init(ctx, "test1", NULL), 0);
	ATF_REQUIRE_EQ(saslc_end(ctx), 0);
}

ATF_TC(t_parser_test2);
ATF_TC_HEAD(t_parser_test2, tc)
{
	atf_tc_set_md_var(tc, "descr", "parser test2");
	set_env(tc);
}
ATF_TC_BODY(t_parser_test2, tc)
{
	saslc_t *ctx;
	saslc_sess_t *sess;
	const char *val;

	ATF_REQUIRE(ctx = saslc_alloc());
	ATF_CHECK_EQ(saslc_init(ctx, "test2", NULL), 0);
	ATF_REQUIRE((sess = saslc_sess_init(ctx, "ANONYMOUS", NULL)));
	ATF_REQUIRE(val = saslc_sess_getprop(sess, "TEST"));
	ATF_CHECK_STREQ(val, "one");
	ATF_REQUIRE(val = saslc_sess_getprop(sess, "TEST2"));
	ATF_CHECK_STREQ(val, "one two");
	ATF_REQUIRE(val = saslc_sess_getprop(sess, "TEST3"));
	ATF_CHECK_STREQ(val, "one two three");
	ATF_REQUIRE(val = saslc_sess_getprop(sess, "ID"));
	ATF_CHECK_STREQ(val, "6669");
	saslc_sess_end(sess);
	ATF_REQUIRE_EQ(saslc_end(ctx), 0);
}

ATF_TC(t_parser_test3);
ATF_TC_HEAD(t_parser_test3, tc)
{
	atf_tc_set_md_var(tc, "descr", "parser test3");
	set_env(tc);
}
ATF_TC_BODY(t_parser_test3, tc)
{
	saslc_t *ctx;
	int r;

	ATF_REQUIRE(ctx = saslc_alloc());
	ATF_CHECK_EQ(saslc_init(ctx, "test3", NULL), -1);
	ATF_REQUIRE_EQ(saslc_end(ctx), 0);
}


ATF_TC(t_parser_test4);
ATF_TC_HEAD(t_parser_test4, tc)
{
	atf_tc_set_md_var(tc, "descr", "parser test4");
}
ATF_TC_BODY(t_parser_test4, tc)
{
	saslc_t *ctx;
	int r;

	ATF_REQUIRE(ctx = saslc_alloc());
	ATF_CHECK_EQ(saslc_init(ctx, "test4", NULL), -1);
	ATF_REQUIRE_EQ(saslc_end(ctx), 0);
}


ATF_TP_ADD_TCS(tp)
{

	setenv(SASLC_ENV_CONFIG, SASLC_TEST_DIR "parser_tests/", 1);
	ATF_TP_ADD_TC(tp, t_parser_test1);
	ATF_TP_ADD_TC(tp, t_parser_test2);
	ATF_TP_ADD_TC(tp, t_parser_test3);
	ATF_TP_ADD_TC(tp, t_parser_test4);

	return atf_no_error();
}
