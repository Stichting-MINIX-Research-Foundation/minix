/*	$NetBSD: t_io.c,v 1.11 2013/06/12 12:08:08 pooka Exp $	*/

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

#include <sys/stat.h>
#include <sys/statvfs.h>

#include <atf-c.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>

#include <rump/rump_syscalls.h>
#include <rump/rump.h>

#include "../common/h_fsmacros.h"
#include "../../h_macros.h"

#define TESTSTR "this is a string.  collect enough and you'll have Em"
#define TESTSZ sizeof(TESTSTR)

static void
holywrite(const atf_tc_t *tc, const char *mp)
{
	char buf[1024];
	char *b2, *b3;
	size_t therange = getpagesize()+1;
	int fd;

	FSTEST_ENTER();

	RL(fd = rump_sys_open("file", O_RDWR|O_CREAT|O_TRUNC, 0666));

	memset(buf, 'A', sizeof(buf));
	RL(rump_sys_pwrite(fd, buf, 1, getpagesize()));

	memset(buf, 'B', sizeof(buf));
	RL(rump_sys_pwrite(fd, buf, 2, getpagesize()-1));

	REQUIRE_LIBC(b2 = malloc(2 * getpagesize()), NULL);
	REQUIRE_LIBC(b3 = malloc(2 * getpagesize()), NULL);

	RL(rump_sys_pread(fd, b2, therange, 0));

	memset(b3, 0, therange);
	memset(b3 + getpagesize() - 1, 'B', 2);

	ATF_REQUIRE_EQ(memcmp(b2, b3, therange), 0);

	rump_sys_close(fd);
	FSTEST_EXIT();
}

static void
extendbody(const atf_tc_t *tc, off_t seekcnt)
{
	char buf[TESTSZ+1];
	struct stat sb;
	int fd;

	FSTEST_ENTER();
	RL(fd = rump_sys_open("testfile",
	    O_CREAT | O_RDWR | (seekcnt ? O_APPEND : 0)));
	RL(rump_sys_ftruncate(fd, seekcnt));
	RL(rump_sys_fstat(fd, &sb));
	ATF_REQUIRE_EQ(sb.st_size, seekcnt);

	ATF_REQUIRE_EQ(rump_sys_write(fd, TESTSTR, TESTSZ), TESTSZ);
	ATF_REQUIRE_EQ(rump_sys_pread(fd, buf, TESTSZ, seekcnt), TESTSZ);
	ATF_REQUIRE_STREQ(buf, TESTSTR);

	RL(rump_sys_fstat(fd, &sb));
	ATF_REQUIRE_EQ(sb.st_size, (off_t)TESTSZ + seekcnt);
	RL(rump_sys_close(fd));
	FSTEST_EXIT();
}

static void
extendfile(const atf_tc_t *tc, const char *mp)
{

	extendbody(tc, 0);
}

static void
extendfile_append(const atf_tc_t *tc, const char *mp)
{

	extendbody(tc, 37);
}

static void
overwritebody(const atf_tc_t *tc, off_t count, bool dotrunc)
{
	char *buf;
	int fd;

	REQUIRE_LIBC(buf = malloc(count), NULL);
	FSTEST_ENTER();
	RL(fd = rump_sys_open("testi", O_CREAT | O_RDWR, 0666));
	ATF_REQUIRE_EQ(rump_sys_write(fd, buf, count), count);
	RL(rump_sys_close(fd));

	RL(fd = rump_sys_open("testi", O_RDWR));
	if (dotrunc)
		RL(rump_sys_ftruncate(fd, 0));
	ATF_REQUIRE_EQ(rump_sys_write(fd, buf, count), count);
	RL(rump_sys_close(fd));
	FSTEST_EXIT();
}

static void
overwrite512(const atf_tc_t *tc, const char *mp)
{

	overwritebody(tc, 512, false);
}

static void
overwrite64k(const atf_tc_t *tc, const char *mp)
{

	overwritebody(tc, 1<<16, false);
}

static void
overwrite_trunc(const atf_tc_t *tc, const char *mp)
{

	overwritebody(tc, 1<<16, true);
}

static void
shrinkfile(const atf_tc_t *tc, const char *mp)
{
	int fd;

	FSTEST_ENTER();
	RL(fd = rump_sys_open("file", O_RDWR|O_CREAT|O_TRUNC, 0666));
	RL(rump_sys_ftruncate(fd, 2));
	RL(rump_sys_ftruncate(fd, 1));
	rump_sys_close(fd);
	FSTEST_EXIT();
}

ATF_TC_FSAPPLY(holywrite, "create a sparse file and fill hole");
ATF_TC_FSAPPLY(extendfile, "check that extending a file works");
ATF_TC_FSAPPLY(extendfile_append, "check that extending a file works "
				  "with a append-only fd (PR kern/44307)");
ATF_TC_FSAPPLY(overwrite512, "write a 512 byte file twice");
ATF_TC_FSAPPLY(overwrite64k, "write a 64k byte file twice");
ATF_TC_FSAPPLY(overwrite_trunc, "write 64k + truncate + rewrite");
ATF_TC_FSAPPLY(shrinkfile, "shrink file");

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_FSAPPLY(holywrite);
	ATF_TP_FSAPPLY(extendfile);
	ATF_TP_FSAPPLY(extendfile_append);
	ATF_TP_FSAPPLY(overwrite512);
	ATF_TP_FSAPPLY(overwrite64k);
	ATF_TP_FSAPPLY(overwrite_trunc);
	ATF_TP_FSAPPLY(shrinkfile);

	return atf_no_error();
}
