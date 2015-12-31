/*	$NetBSD: ttyname.c,v 1.26 2012/06/12 18:17:04 joerg Exp $	*/

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
__RCSID("$NetBSD: ttyname.c,v 1.26 2012/06/12 18:17:04 joerg Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(ttyname,_ttyname)
__weak_alias(ttyname_r,_ttyname_r)
#endif

int
ttyname_r(int fd, char *buf, size_t len)
{
	struct stat sb;
	struct termios ttyb;
	struct ptmget ptm;

	_DIAGASSERT(fd != -1);

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

	if (fstat(fd, &sb))
		return errno;

	if (strlcpy(buf, _PATH_DEV, len) >= len)
		return ERANGE;
	buf += strlen(_PATH_DEV);
	len -= strlen(_PATH_DEV);
	return devname_r(sb.st_rdev, sb.st_mode & S_IFMT, buf, len);
}

char *
ttyname(int fd)
{
	static char buf[PATH_MAX];
	int rv;
	
	rv = ttyname_r(fd, buf, sizeof(buf));
	if (rv != 0) {
		errno = rv;
		return NULL;
	}
	return buf;
}
