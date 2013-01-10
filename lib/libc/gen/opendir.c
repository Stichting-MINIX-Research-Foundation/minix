/*	$NetBSD: opendir.c,v 1.38 2011/10/15 23:00:01 christos Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
static char sccsid[] = "@(#)opendir.c	8.7 (Berkeley) 12/10/94";
#else
__RCSID("$NetBSD: opendir.c,v 1.38 2011/10/15 23:00:01 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"

#ifdef __minix
#include <sys/cdefs.h>
#include <sys/types.h>

#endif

#include "extern.h"

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dirent_private.h"

static DIR	*__opendir_common(int, const char *, int);

__weak_alias(fdopendir,_fdopendir)

#if defined(__weak_alias) && defined(__minix)
__weak_alias(opendir,__opendir230)
#endif

/*
 * Open a directory.
 */
DIR *
opendir(const char *name)
{

	_DIAGASSERT(name != NULL);

	return (__opendir2(name, DTF_HIDEW|DTF_NODUP));
}

DIR *
__opendir2(const char *name, int flags)
{
	int fd;

	if ((fd = open(name, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) == -1)
		return NULL;

	return __opendir_common(fd, name, flags);
}

#ifndef __LIBC12_SOURCE__
DIR *
_fdopendir(int fd)
{
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		return NULL;

	return __opendir_common(fd, NULL, DTF_HIDEW|DTF_NODUP);
}
#endif

static DIR *
__opendir_common(int fd, const char *name, int flags)
{
	DIR *dirp = NULL;
	int serrno;
	struct stat sb;
#ifndef __minix
	struct statvfs sfb;
#endif
	int error;

	if (fstat(fd, &sb) || !S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		goto error;
	}
	if ((dirp = malloc(sizeof(*dirp))) == NULL)
		goto error;
	dirp->dd_buf = NULL;
	dirp->dd_internal = NULL;
#ifdef _REENTRANT
	if (__isthreaded) {
		if ((dirp->dd_lock = malloc(sizeof(mutex_t))) == NULL)
			goto error;
		mutex_init((mutex_t *)dirp->dd_lock, NULL);
	}
#endif

	/*
	 * Tweak flags for the underlying filesystem.
	 */

#ifdef __minix
	/* MOUNT_UNION and MOUNT_NFS not supported */
	flags &= ~DTF_NODUP;
#else
	if (fstatvfs1(fd, &sfb, ST_NOWAIT) < 0)
		goto error;
	if ((flags & DTF_NODUP) != 0) {
		if (!strncmp(sfb.f_fstypename, MOUNT_UNION,
		    sizeof(sfb.f_fstypename)) ||
		    (sfb.f_flag & MNT_UNION) != 0) {
			flags |= __DTF_READALL;
		} else {
			flags &= ~DTF_NODUP;
		}
	}
	if (!strncmp(sfb.f_fstypename, MOUNT_NFS, sizeof(sfb.f_fstypename))) {
		flags |= __DTF_READALL | __DTF_RETRY_ON_BADCOOKIE;
	}
#endif

	dirp->dd_flags = flags;
	error = _initdir(dirp, fd, name);
	if (error) {
		errno = error;
		goto error;
	}

	return (dirp);
error:
	serrno = errno;
	if (dirp != NULL) {
#ifdef _REENTRANT
		if (__isthreaded) {
			mutex_destroy((mutex_t *)dirp->dd_lock);
			free(dirp->dd_lock);
		}
#endif
		free(dirp->dd_buf);
	}
	free(dirp);
	if (fd != -1)
		(void)close(fd);
	errno = serrno;
	return NULL;
}
