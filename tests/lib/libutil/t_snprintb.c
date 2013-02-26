/* $NetBSD: t_snprintb.c,v 1.1 2010/07/16 13:56:32 jmmv Exp $ */

/*
 * Copyright (c) 2002, 2004, 2008, 2010 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2008, 2010\
 The NetBSD Foundation, inc. All rights reserved.");
__RCSID("$NetBSD: t_snprintb.c,v 1.1 2010/07/16 13:56:32 jmmv Exp $");

#include <string.h>
#include <util.h>

#include <atf-c.h>

static void
h_snprintb(const char *fmt, uint64_t val, const char *res)
{
	char buf[1024];
	int len, slen;

	len = snprintb(buf, sizeof(buf), fmt, val);
	slen = (int) strlen(res);

	ATF_REQUIRE_STREQ(res, buf);
	ATF_REQUIRE_EQ(len, slen);
}

ATF_TC(snprintb);
ATF_TC_HEAD(snprintb, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks snprintb(3)");
}
ATF_TC_BODY(snprintb, tc)
{
	h_snprintb("\10\2BITTWO\1BITONE", 3, "03<BITTWO,BITONE>");

	h_snprintb("\177\20b\05NOTBOOT\0b\06FPP\0b\013SDVMA\0b\015VIDEO\0"
		"b\020LORES\0b\021FPA\0b\022DIAG\0b\016CACHE\0"
		"b\017IOCACHE\0b\022LOOPBACK\0b\04DBGCACHE\0",
		0xe860, "0xe860<NOTBOOT,FPP,SDVMA,VIDEO,CACHE,IOCACHE>");
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, snprintb);

	return atf_no_error();
}
