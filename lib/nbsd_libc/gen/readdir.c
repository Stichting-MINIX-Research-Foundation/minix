/*	$NetBSD: readdir.c,v 1.25 2010/09/16 02:38:50 yamt Exp $	*/

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
static char sccsid[] = "@(#)readdir.c	8.3 (Berkeley) 9/29/94";
#else
__RCSID("$NetBSD: readdir.c,v 1.25 2010/09/16 02:38:50 yamt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"
#include "extern.h"
#include <sys/param.h>

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "dirent_private.h"

/*
 * get next entry in a directory.
 */
struct dirent *
_readdir_unlocked(DIR *dirp, int skipdeleted)
{
	struct dirent *dp;

	for (;;) {
		if (dirp->dd_loc >= dirp->dd_size) {
			if (dirp->dd_flags & __DTF_READALL)
				return (NULL);
			dirp->dd_loc = 0;
		}
		if (dirp->dd_loc == 0 && !(dirp->dd_flags & __DTF_READALL)) {
			dirp->dd_seek = lseek(dirp->dd_fd, (off_t)0, SEEK_CUR);
			dirp->dd_size = getdents(dirp->dd_fd,
			    dirp->dd_buf, (size_t)dirp->dd_len);
			if (dirp->dd_size <= 0)
				return (NULL);
		}
		dp = (struct dirent *)
		    (void *)(dirp->dd_buf + (size_t)dirp->dd_loc);
		if ((intptr_t)dp & _DIRENT_ALIGN(dp))/* bogus pointer check */
			return (NULL);
		/* d_reclen is unsigned; no need to compare it <= 0 */
		if (dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc)
			return (NULL);
		dirp->dd_loc += dp->d_reclen;
		if (dp->d_ino == 0 && skipdeleted)
			continue;
		if (dp->d_type == DT_WHT && (dirp->dd_flags & DTF_HIDEW))
			continue;
		return (dp);
	}
}

struct dirent *
readdir(dirp)
	DIR *dirp;
{
	struct dirent	*dp;

#ifdef _REENTRANT
	if (__isthreaded) {
		mutex_lock((mutex_t *)dirp->dd_lock);
		dp = _readdir_unlocked(dirp, 1);
		mutex_unlock((mutex_t *)dirp->dd_lock);
	}
	else
#endif
		dp = _readdir_unlocked(dirp, 1);
	return (dp);
}

int
readdir_r(dirp, entry, result)
	DIR *dirp;
	struct dirent *entry;
	struct dirent **result;
{
	struct dirent *dp;
	int saved_errno;

	saved_errno = errno;
	errno = 0;
#ifdef _REENTRANT
	if (__isthreaded) {
		mutex_lock((mutex_t *)dirp->dd_lock);
		if ((dp = _readdir_unlocked(dirp, 1)) != NULL)
			memcpy(entry, dp, (size_t)_DIRENT_SIZE(dp));
		mutex_unlock((mutex_t *)dirp->dd_lock);
	}
	else 
#endif
		if ((dp = _readdir_unlocked(dirp, 1)) != NULL)
			memcpy(entry, dp, (size_t)_DIRENT_SIZE(dp));

	if (errno != 0) {
		if (dp == NULL)
			return (errno);
	} else
		errno = saved_errno;

	if (dp != NULL)
		*result = entry;
	else
		*result = NULL;

	return (0);
}
