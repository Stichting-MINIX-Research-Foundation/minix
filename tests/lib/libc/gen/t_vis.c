/*	$NetBSD: t_vis.c,v 1.6 2013/02/13 04:51:56 christos Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code was contributed to The NetBSD Foundation by Christos Zoulas.
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

#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <vis.h>

static int styles[] = {
	VIS_OCTAL,
	VIS_CSTYLE,
	VIS_SP,
	VIS_TAB,
	VIS_NL,
	VIS_WHITE,
	VIS_SAFE,
#if 0	/* Not reversible */
	VIS_NOSLASH,
#endif
	VIS_HTTP1808,
	VIS_MIMESTYLE,
#if 0	/* Not supported by vis(3) */
	VIS_HTTP1866,
#endif
};

#define SIZE	256

ATF_TC(strvis_basic);
ATF_TC_HEAD(strvis_basic, tc)
{

	atf_tc_set_md_var(tc, "descr", "Test strvis(3)");
}

ATF_TC_BODY(strvis_basic, tc)
{
	char *srcbuf, *dstbuf, *visbuf;
	unsigned int i, j;

	ATF_REQUIRE((dstbuf = malloc(SIZE)) != NULL);
	ATF_REQUIRE((srcbuf = malloc(SIZE)) != NULL);
	ATF_REQUIRE((visbuf = malloc(SIZE * 4 + 1)) != NULL);

	for (i = 0; i < SIZE; i++)
		srcbuf[i] = (char)i;

	for (i = 0; i < __arraycount(styles); i++) {
		ATF_REQUIRE(strsvisx(visbuf, srcbuf, SIZE, styles[i], "") > 0);
		memset(dstbuf, 0, SIZE);
		ATF_REQUIRE(strunvisx(dstbuf, visbuf, 
		    styles[i] & (VIS_HTTP1808|VIS_MIMESTYLE)) > 0);
		for (j = 0; j < SIZE; j++)
			if (dstbuf[j] != (char)j)
				atf_tc_fail_nonfatal("Failed for style %x, "
				    "char %d [%d]", styles[i], j, dstbuf[j]);
	}
	free(dstbuf);
	free(srcbuf);
	free(visbuf);
}

ATF_TC(strunvis_hex);
ATF_TC_HEAD(strunvis_hex, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test strunvis(3) \\xXX");
}

ATF_TC_BODY(strunvis_hex, tc)
{
	static const struct {
		const char *e;
		const char *d;
		int error;
	} ed[] = {
		{ "\\xff", "\xff", 1 },
		{ "\\x1", "\x1", 1 },
		{ "\\x1\\x02", "\x1\x2", 2 },
		{ "\\x1x", "\x1x", 2 },
		{ "\\xx", "", -1 },
	};
	char uv[10];

	for (size_t i = 0; i < __arraycount(ed); i++) {
		ATF_REQUIRE(strunvis(uv, ed[i].e) == ed[i].error);
		if (ed[i].error > 0)
			ATF_REQUIRE(memcmp(ed[i].d, uv, ed[i].error) == 0);
	}
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, strvis_basic);
	ATF_TP_ADD_TC(tp, strunvis_hex);

	return atf_no_error();
}
