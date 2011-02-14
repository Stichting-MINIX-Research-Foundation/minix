/*	$NetBSD: ttyname.c,v 1.24 2008/06/25 11:47:29 ad Exp $	*/

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
static char sccsid[] = "@(#)ttyname.c	8.2 (Berkeley) 1/27/94";
#else
__RCSID("$NetBSD: ttyname.c,v 1.24 2008/06/25 11:47:29 ad Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <db.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __weak_alias
__weak_alias(ttyname,_ttyname)
__weak_alias(ttyname_r,_ttyname_r)
#endif

static int oldttyname(const struct stat *, char *, size_t);

int
ttyname_r(int fd, char *buf, size_t len)
{
	struct stat sb;
	struct termios ttyb;
	DB *db;
	DBT data, key;
	struct {
		mode_t type;
		dev_t dev;
	} bkey;
	struct ptmget ptm;
#define DEVSZ (sizeof(_PATH_DEV) - 1)

	_DIAGASSERT(fd != -1);

	if (len <= DEVSZ) {
		return ERANGE;
	}

	/* If it is a pty, deal with it quickly */
	if (ioctl(fd, TIOCPTSNAME, &ptm) != -1) {
		if (strlcpy(buf, ptm.sn, len) >= len) {
			return ERANGE;
		}
		return 0;
	}
	/* Must be a terminal. */
	if (tcgetattr(fd, &ttyb) == -1)
		return errno;

	/* Must be a character device. */
	if (fstat(fd, &sb))
		return errno;
	if (!S_ISCHR(sb.st_mode))
		return ENOTTY;

	(void)memcpy(buf, _PATH_DEV, DEVSZ);
	if ((db = dbopen(_PATH_DEVDB, O_RDONLY, 0, DB_HASH, NULL)) != NULL) {
		(void)memset(&bkey, 0, sizeof(bkey));
		bkey.type = S_IFCHR;
		bkey.dev = sb.st_rdev;
		key.data = &bkey;
		key.size = sizeof(bkey);
		if (!(db->get)(db, &key, &data, 0)) {
			if (len - DEVSZ <= data.size) {
				return ERANGE;
			}
			(void)memcpy(buf + DEVSZ, data.data, data.size);
			(void)(db->close)(db);
			return 0;
		}
		(void)(db->close)(db);
	}
	if (oldttyname(&sb, buf, len) == -1)
		return errno;
	return 0;
}

static int
oldttyname(const struct stat *sb, char *buf, size_t len)
{
	struct dirent *dirp;
	DIR *dp;
	struct stat dsb;
	size_t dlen;

	_DIAGASSERT(sb != NULL);

	if ((dp = opendir(_PATH_DEV)) == NULL)
		return -1;

	while ((dirp = readdir(dp)) != NULL) {
		if (dirp->d_fileno != sb->st_ino)
			continue;
		dlen = dirp->d_namlen + 1;
		if (len - DEVSZ <= dlen) {
			/*
			 * XXX: we return an error if *any* entry does not
			 * fit
			 */
			errno = ERANGE;
			(void)closedir(dp);
			return -1;
		}
		(void)memcpy(buf + DEVSZ, dirp->d_name, dlen);
		if (stat(buf, &dsb) || sb->st_dev != dsb.st_dev ||
		    sb->st_ino != dsb.st_ino)
			continue;
		(void)closedir(dp);
		return 0;
	}
	(void)closedir(dp);
	/*
	 * XXX: Documented by TOG to return EBADF or ENOTTY only; neither are
	 * applicable here.
	 */
	errno = ENOENT;
	return -1;
}

char *
ttyname(int fd)
{
	static char buf[MAXPATHLEN];
	int rv;
	
	rv = ttyname_r(fd, buf, sizeof(buf));
	if (rv != 0) {
		errno = rv;
		return NULL;
	}
	return buf;
}
