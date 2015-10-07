/* $NetBSD: t_session.c,v 1.4 2011/02/12 23:21:33 christos Exp $ */

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
__RCSID("$NetBSD: t_session.c,v 1.4 2011/02/12 23:21:33 christos Exp $");

#include <atf-c.h>
#include <saslc.h>
#include <stdio.h>

ATF_TC(t_sess);
ATF_TC_HEAD(t_sess, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc_sess_*() tests");
}
ATF_TC_BODY(t_sess, tc)
{
	saslc_t *ctx;
	saslc_sess_t *sess;

	ATF_REQUIRE(ctx = saslc_alloc());
	ATF_REQUIRE_EQ(saslc_init(ctx, NULL, NULL), 0);
	ATF_CHECK(sess = saslc_sess_init(ctx, "PLAIN", NULL));
	if (sess != NULL)
		saslc_sess_end(sess);
	ATF_CHECK_EQ(sess = saslc_sess_init(ctx, "", NULL), NULL);
	ATF_CHECK(sess = saslc_sess_init(ctx, "NOTEXISTS,LOGIN", NULL));
	if (sess != NULL)
		saslc_sess_end(sess);
	ATF_CHECK(sess = saslc_sess_init(ctx, "LOGIN,NOTEXISTS", NULL));
	if (sess != NULL)
		saslc_sess_end(sess);
	ATF_REQUIRE(sess = saslc_sess_init(ctx, "LOGiN", NULL));
	ATF_CHECK_STREQ(saslc_sess_getmech(sess), "LOGIN");
	ATF_REQUIRE_EQ(saslc_end(ctx), -1);
	saslc_sess_end(sess);
	ATF_REQUIRE_EQ(saslc_end(ctx), 0);
}

ATF_TP_ADD_TCS(tp)
{

	/* context initialization */
	ATF_TP_ADD_TC(tp, t_sess);
	return atf_no_error();
}
