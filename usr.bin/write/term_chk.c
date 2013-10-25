/* $NetBSD: term_chk.c,v 1.8 2009/04/14 07:59:17 lukem Exp $ */

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jef Poskanzer and Craig Leres of the Lawrence Berkeley Laboratory.
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
#ifndef lint
__RCSID("$NetBSD: term_chk.c,v 1.8 2009/04/14 07:59:17 lukem Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <paths.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>

#include "term_chk.h"

/*
 * term_chk - check that a terminal exists, and get the message bit
 *     and the access time
 */
int
term_chk(uid_t uid, const char *tty, int *msgsokP, time_t *atimeP, int ismytty,
    gid_t saved_egid)
{
	char path[MAXPATHLEN];
	struct stat s;
	int i, fd, serrno;

	if (strstr(tty, "../") != NULL) {
		errno = EINVAL;
		return -1;
	}
	i = snprintf(path, sizeof path, _PATH_DEV "%s", tty);
	if (i < 0 || i >= (int)sizeof(path)) {
		errno = ENOMEM;
		return -1;
	}

	(void)setegid(saved_egid);
	fd = open(path, O_WRONLY, 0);
	serrno = errno;
	(void)setegid(getgid());
	errno = serrno;

	if (fd == -1)
		return(-1);
	if (fstat(fd, &s) == -1)
		goto error;
	if (!isatty(fd))
		goto error;
	if (s.st_uid != uid && uid != 0) {
		errno = EPERM;
		goto error;
	}
	if (msgsokP)
		*msgsokP = (s.st_mode & S_IWGRP) != 0;	/* group write bit */
	if (atimeP)
		*atimeP = s.st_atime;
	if (ismytty)
		(void)close(fd);
	return ismytty ? 0 : fd;
error:
	if (fd != -1) {
		serrno = errno;
		(void)close(fd);
		errno = serrno;
	}
	return -1;
}

char *
check_sender(time_t *atime, uid_t myuid, gid_t saved_egid)
{
	int myttyfd;
	int msgsok;
	char *mytty;

	/* check that sender has write enabled */
	if (isatty(fileno(stdin)))
		myttyfd = fileno(stdin);
	else if (isatty(fileno(stdout)))
		myttyfd = fileno(stdout);
	else if (isatty(fileno(stderr)))
		myttyfd = fileno(stderr);
	else if (atime == NULL)
		return NULL;
	else
		errx(1, "Cannot find your tty");
	if ((mytty = ttyname(myttyfd)) == NULL)
		err(1, "Cannot find the name of your tty");
	if (strncmp(mytty, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
		mytty += sizeof(_PATH_DEV) - 1;
	if (term_chk(myuid, mytty, &msgsok, atime, 1, saved_egid) == -1)
		err(1, "%s%s", _PATH_DEV, mytty);
	if (!msgsok) {
		warnx(
		    "You have write permission turned off; no reply possible");
	}
	return mytty;
}
