/* $NetBSD: t_dict.c,v 1.4 2011/02/12 23:21:33 christos Exp $ */

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
__RCSID("$NetBSD: t_dict.c,v 1.4 2011/02/12 23:21:33 christos Exp $");

#include <atf-c.h>
#include <stdio.h>

#include "dict.h"

/* src/dict.c test cases */

/* saslc__dict_create() */
ATF_TC(t_saslc__dict_create);
ATF_TC_HEAD(t_saslc__dict_create, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc__dict_create() tests");
}
ATF_TC_BODY(t_saslc__dict_create, tc)
{

	saslc__dict_t *dict;
	ATF_REQUIRE(dict = saslc__dict_create());
	saslc__dict_destroy(dict);
}

/* saslc__dict_insert() */
ATF_TC(t_saslc__dict_insert);
ATF_TC_HEAD(t_saslc__dict_insert, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc__dict_insert() tests");
}
ATF_TC_BODY(t_saslc__dict_insert, tc)
{

	saslc__dict_t *dict;
	ATF_REQUIRE(dict = saslc__dict_create());
	ATF_CHECK_EQ(saslc__dict_insert(dict, "foo", "bar"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_insert(dict, "bar", "blah"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_insert(dict, " ", "bar"), DICT_KEYINVALID);
	ATF_CHECK_EQ(saslc__dict_insert(dict, NULL, NULL), DICT_KEYINVALID);
	ATF_CHECK_EQ(saslc__dict_insert(dict, "a", NULL), DICT_VALBAD);
	ATF_CHECK_EQ(saslc__dict_insert(dict,
	    "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890",
	     "zero"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_insert(dict, "a", "b"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_insert(dict, "a", "c"), DICT_KEYEXISTS);
	ATF_CHECK_EQ(saslc__dict_insert(dict, "foo", "bar"), DICT_KEYEXISTS);
	ATF_CHECK_EQ(saslc__dict_insert(dict, "&^#%$#", "bad"), DICT_KEYINVALID);
	saslc__dict_destroy(dict);
}

/* saslc__dict_remove() */
ATF_TC(t_saslc__dict_remove);
ATF_TC_HEAD(t_saslc__dict_remove, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc__dict_remove() tests");
}
ATF_TC_BODY(t_saslc__dict_remove, tc)
{

	saslc__dict_t *dict;
	ATF_REQUIRE(dict = saslc__dict_create());
	ATF_CHECK_EQ(saslc__dict_remove(dict, "BAR"), DICT_KEYNOTFOUND);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo", "bar"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "BAR"), DICT_KEYNOTFOUND);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "BAR", "bar"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "BAR"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "BAR"), DICT_KEYNOTFOUND);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "BAR", "bar"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "foo"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "BAR"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo", "bar"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "foo"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo1", "bar"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo2", "bar"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo3", "bar"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "foo2"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "foo1"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "foo3"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_remove(dict, "foo3"), DICT_KEYNOTFOUND);
	saslc__dict_destroy(dict);
}

/* saslc__dict_get() */
ATF_TC(t_saslc__dict_get);
ATF_TC_HEAD(t_saslc__dict_get, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc__dict_get() tests");
}
ATF_TC_BODY(t_saslc__dict_get, tc)
{

	saslc__dict_t *dict;
	ATF_REQUIRE(dict = saslc__dict_create());
	ATF_CHECK_EQ(saslc__dict_get(dict, "BAR"), NULL);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo1", "bar1"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo2", "bar2"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo3", "bar3"), DICT_OK);
	ATF_CHECK_STREQ(saslc__dict_get(dict, "foo1"), "bar1");
	ATF_CHECK_STREQ(saslc__dict_get(dict, "foo2"), "bar2");
	ATF_CHECK_STREQ(saslc__dict_get(dict, "foo3"), "bar3");
	ATF_CHECK_EQ(saslc__dict_get(dict, "foo4"), NULL);
	ATF_REQUIRE_EQ(saslc__dict_remove(dict, "foo2"), DICT_OK);
	ATF_CHECK_STREQ(saslc__dict_get(dict, "foo1"), "bar1");
	ATF_CHECK_EQ(saslc__dict_get(dict, "foo2"), NULL);
	ATF_CHECK_STREQ(saslc__dict_get(dict, "foo3"), "bar3");
	ATF_REQUIRE_EQ(saslc__dict_remove(dict, "foo1"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_remove(dict, "foo3"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_get(dict, "foo2"), NULL);
	saslc__dict_destroy(dict);
}

/* saslc__dict_get_len() */
ATF_TC(t_saslc__dict_get_len);
ATF_TC_HEAD(t_saslc__dict_get_len, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc__dict_get_len() tests");
}
ATF_TC_BODY(t_saslc__dict_get_len, tc)
{

	saslc__dict_t *dict;
	ATF_REQUIRE(dict = saslc__dict_create());
	ATF_CHECK_EQ(saslc__dict_get_len(dict, "BAR"), 0);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo1", "1"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo2", "1234567890"), DICT_OK);
	ATF_REQUIRE_EQ(saslc__dict_insert(dict, "foo3", "12345678901234567890"), DICT_OK);
	ATF_CHECK_EQ(saslc__dict_get_len(dict, "foo4"), 0);
	ATF_CHECK_EQ(saslc__dict_get_len(dict, "foo1"), 1);
	ATF_CHECK_EQ(saslc__dict_get_len(dict, "foo2"), 10);
	ATF_CHECK_EQ(saslc__dict_get_len(dict, "foo3"), 20);
	saslc__dict_destroy(dict);
}

ATF_TP_ADD_TCS(tp)
{
	/* constructors and destructors */
	ATF_TP_ADD_TC(tp, t_saslc__dict_create);

	/* modifiers */
	ATF_TP_ADD_TC(tp, t_saslc__dict_insert);
	ATF_TP_ADD_TC(tp, t_saslc__dict_remove);

	/* getters */
	ATF_TP_ADD_TC(tp, t_saslc__dict_get);
	ATF_TP_ADD_TC(tp, t_saslc__dict_get_len);

	return atf_no_error();
}
