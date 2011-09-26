/*	$NetBSD: logout.c,v 1.16 2005/08/27 17:07:17 elad Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)logout.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: logout.c,v 1.16 2005/08/27 17:07:17 elad Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <utmp.h>

int
logout(const char *line)
{
	int fd, rval;
	struct utmp ut;

	_DIAGASSERT(line != NULL);

	if ((fd = open(_PATH_UTMP, O_RDWR, 0)) < 0)
		return(0);
	rval = 0;
	while (read(fd, &ut, sizeof(ut)) == sizeof(ut)) {
		if (!ut.ut_name[0] || strncmp(ut.ut_line, line,
					      (size_t)UT_LINESIZE))
			continue;
		memset(ut.ut_name, 0, (size_t)UT_NAMESIZE);
		memset(ut.ut_host, 0, (size_t)UT_HOSTSIZE);
		(void)time(&ut.ut_time);
		(void)lseek(fd, -(off_t)sizeof(ut), SEEK_CUR);
		(void)write(fd, &ut, sizeof(ut));
		rval = 1;
	}
	(void)close(fd);
	return(rval);
}
