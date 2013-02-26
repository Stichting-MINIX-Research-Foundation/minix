/*	$NetBSD: t_unix.c,v 1.6 2011/10/04 16:28:26 christos Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__RCSID("$Id: t_unix.c,v 1.6 2011/10/04 16:28:26 christos Exp $");

#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef TEST
#define FAIL(msg, ...)	err(EXIT_FAILURE, msg, ## __VA_ARGS__)
#else

#include <atf-c.h>
#define FAIL(msg, ...)	ATF_CHECK_MSG(0, msg, ## __VA_ARGS__)

#endif

static __dead int
acc(int s)
{
	char guard1;
	struct sockaddr_un sun;
	char guard2;
	socklen_t len;

	guard1 = guard2 = 's';

	len = sizeof(sun);
	if (accept(s, (struct sockaddr *)&sun, &len) == -1)
		FAIL("accept");
	if (guard1 != 's')
		errx(EXIT_FAILURE, "guard1 = '%c'", guard1);
	if (guard2 != 's')
		errx(EXIT_FAILURE, "guard2 = '%c'", guard2);
	close(s);
	exit(0);
}

static int
test(size_t len)
{
	struct sockaddr_un *sun;
	int s, s2;
	size_t slen;
	socklen_t sl;

	slen = len + offsetof(struct sockaddr_un, sun_path) + 1;
	
	if ((sun = calloc(1, slen)) == NULL)
		FAIL("calloc");

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s == -1)
		FAIL("socket");

	memset(sun->sun_path, 'a', len);
	sun->sun_path[len] = '\0';
	(void)unlink(sun->sun_path);

	sl = SUN_LEN(sun);
	sun->sun_len = sl;
	sun->sun_family = AF_UNIX;

	if (bind(s, (struct sockaddr *)sun, sl) == -1) {
		if (errno == EINVAL && sl >= 256)
			return -1;
		FAIL("bind");
	}

	if (listen(s, 5) == -1)
		FAIL("listen");

	switch (fork()) {
	case -1:
		FAIL("fork");
	case 0:
		acc(s);
		/*NOTREACHED*/
	default:
		sleep(1);
		s2 = socket(AF_UNIX, SOCK_STREAM, 0);
		if (s2 == -1)
			FAIL("socket");
		if (connect(s2, (struct sockaddr *)sun, sl) == -1)
			FAIL("connect");
		close(s2);
		break;
	}
	return 0;
}

#ifndef TEST

ATF_TC(sockaddr_un_len_exceed);
ATF_TC_HEAD(sockaddr_un_len_exceed, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check that exceeding the size of "
	    "unix domain sockets does not trash memory or kernel when "
	    "exceeding the size of the fixed sun_path");
}

ATF_TC_BODY(sockaddr_un_len_exceed, tc)
{
	ATF_REQUIRE_MSG(test(254) == -1, "test(254): %s", strerror(errno));
}

ATF_TC(sockaddr_un_len_max);
ATF_TC_HEAD(sockaddr_un_len_max, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check that we can use the maximum "
	    "unix domain socket pathlen (253): 255 - sizeof(sun_len) - "
	    "sizeof(sun_family)");
}

ATF_TC_BODY(sockaddr_un_len_max, tc)
{
	ATF_REQUIRE_MSG(test(253) == 0, "test(253): %s", strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, sockaddr_un_len_exceed);
	ATF_TP_ADD_TC(tp, sockaddr_un_len_max);
	return atf_no_error();
}
#else
int
main(int argc, char *argv[])
{
	size_t len;

	if (argc == 1) {
		fprintf(stderr, "Usage: %s <len>\n", getprogname());
		return EXIT_FAILURE;
	}
	test(atoi(argv[1]));
}
#endif
