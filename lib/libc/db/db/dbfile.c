/*	$NetBSD: dbfile.c,v 1.1 2013/12/01 00:22:48 christos Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: dbfile.c,v 1.1 2013/12/01 00:22:48 christos Exp $");

#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <paths.h>
#include <db.h>

int
__dbopen(const char *file, int flags, mode_t mode, struct stat *sb)
{
	int fd;
	int serrno;

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

	if ((fd = open(file, flags | O_CLOEXEC, mode)) == -1)
		return -1;

#if O_CLOEXEC == 0
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		goto out;
#endif

	if (sb && fstat(fd, sb) == -1)
		goto out;

	return fd;
out:
	serrno = errno;
	close(fd);
	errno = serrno;
	return -1;
	
}

int
__dbtemp(const char *prefix, struct stat *sb)
{
	sigset_t set, oset;
	int len;
	int fd, serrno;
	char *envtmp;
	char path[PATH_MAX];

	if (issetugid())
		envtmp = NULL;
	else
		envtmp = getenv("TMPDIR");

	len = snprintf(path, sizeof(path), "%s/%sXXXXXX",
	    envtmp ? envtmp : _PATH_TMP, prefix);
	if ((size_t)len >= sizeof(path)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	
	(void)sigfillset(&set);
	(void)sigprocmask(SIG_BLOCK, &set, &oset);

	if ((fd = mkstemp(path)) != -1) {
		if (unlink(path) == -1)
			goto out;

		if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
			goto out;

		if (sb && fstat(fd, sb) == -1)
			goto out;
	}
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	return fd;
out:
	serrno = errno;
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	close(fd);
	errno = serrno;
	return -1;
}
