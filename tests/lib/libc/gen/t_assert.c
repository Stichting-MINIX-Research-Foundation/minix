/* $NetBSD: t_assert.c,v 1.2 2011/06/14 05:28:00 jruoho Exp $ */

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
__RCSID("$NetBSD: t_assert.c,v 1.2 2011/06/14 05:28:00 jruoho Exp $");

#include <sys/wait.h>

#include <assert.h>
#include <atf-c.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void		handler(int);

static void
handler(int signo)
{
	/* Nothing. */
}

ATF_TC(assert_false);
ATF_TC_HEAD(assert_false, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test that assert(3) works, #1");
}

ATF_TC_BODY(assert_false, tc)
{
	struct sigaction sa;
	pid_t pid;
	int sta;

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {

		(void)closefrom(0);
		(void)memset(&sa, 0, sizeof(struct sigaction));

		sa.sa_flags = 0;
		sa.sa_handler = handler;

		(void)sigemptyset(&sa.sa_mask);
		(void)sigaction(SIGABRT, &sa, 0);

		assert(1 == 1);

		_exit(EXIT_SUCCESS);
	}

	(void)wait(&sta);

	if (WIFSIGNALED(sta) != 0 || WIFEXITED(sta) == 0)
		atf_tc_fail("assert(3) fired haphazardly");
}

ATF_TC(assert_true);
ATF_TC_HEAD(assert_true, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test that assert(3) works, #2");
}

ATF_TC_BODY(assert_true, tc)
{
	struct sigaction sa;
	pid_t pid;
	int sta;

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {

		(void)closefrom(0);
		(void)memset(&sa, 0, sizeof(struct sigaction));

		sa.sa_flags = 0;
		sa.sa_handler = handler;

		(void)sigemptyset(&sa.sa_mask);
		(void)sigaction(SIGABRT, &sa, 0);

		assert(1 == 2);

		_exit(EXIT_SUCCESS);
	}

	(void)wait(&sta);

	if (WIFSIGNALED(sta) == 0 || WTERMSIG(sta) != SIGABRT)
		atf_tc_fail("assert(3) did not fire");
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, assert_false);
	ATF_TP_ADD_TC(tp, assert_true);

	return atf_no_error();
}
