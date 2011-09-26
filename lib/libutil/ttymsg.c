/*	$NetBSD: ttymsg.c,v 1.23 2009/01/18 12:13:04 lukem Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
static char sccsid[] = "@(#)ttymsg.c	8.2 (Berkeley) 11/16/93";
#else
__RCSID("$NetBSD: ttymsg.c,v 1.23 2009/01/18 12:13:04 lukem Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/uio.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

/*
 * Display the contents of a uio structure on a terminal.  Used by wall(1),
 * syslogd(8), and talkd(8).  Forks and finishes in child if write would block,
 * waiting up to tmout seconds.  Returns pointer to error string on unexpected
 * error; string is not newline-terminated.  Various "normal" errors are
 * ignored (exclusive-use, lack of permission, etc.).
 */
char *
ttymsg(struct iovec *iov, int iovcnt, const char *line, int tmout)
{
	static char errbuf[1024];
	char device[MAXNAMLEN];
	const char *ptr;
	int fd, ret;
	struct iovec localiov[32];
	sigset_t nset;
	int forked = 0;
	size_t cnt, left, wret;

	_DIAGASSERT(iov != NULL);
	_DIAGASSERT(iovcnt >= 0);
	_DIAGASSERT(line != NULL);

	if (iovcnt < 0) {
		(void)snprintf(errbuf, sizeof(errbuf),
		    "%s: negative iovcnt", __func__);
		return errbuf;
	}

	if ((size_t)iovcnt >= sizeof(localiov) / sizeof(localiov[0])) {
		(void)snprintf(errbuf, sizeof(errbuf),
		    "%s: too many iov's (%d) max is %zu", __func__,
		    iovcnt, sizeof(localiov) / sizeof(localiov[0]));
		return errbuf;
	}

	ptr = strncmp(line, "pts/", (size_t)4) == 0 ? line + 4 : line;
	if (strcspn(ptr, "./") != strlen(ptr)) {
		/* A slash or dot is an attempt to break security... */
		(void)snprintf(errbuf, sizeof(errbuf),
		    "%s: '/' or '.' in \"%s\"", __func__, line);
		return errbuf;
	}
	ret = snprintf(device, sizeof(device), "%s%s", _PATH_DEV, line);
	if (ret == -1 || ret >= (int)sizeof(device)) {
		(void) snprintf(errbuf, sizeof(errbuf),
		    "%s: line `%s' too long", __func__, line);
		return errbuf;
	}
	cnt = (size_t)ret;

	/*
	 * open will fail on slip lines or exclusive-use lines
	 * if not running as root; not an error.
	 */
	if ((fd = open(device, O_WRONLY|O_NONBLOCK, 0)) < 0) {
		if (errno == EBUSY || errno == EACCES)
			return NULL;
		(void)snprintf(errbuf, sizeof(errbuf),
		    "%s: Cannot open `%s' (%s)",
		    __func__, device, strerror(errno));
		return errbuf;
	}
	if (!isatty(fd)) {
		(void)snprintf(errbuf, sizeof(errbuf),
		    "%s: line `%s' is not a tty device", __func__, device);
		(void)close(fd);
		return errbuf;
	}

	for (cnt = left = 0; cnt < (size_t)iovcnt; ++cnt)
		left += iov[cnt].iov_len;

	for (;;) {
		wret = writev(fd, iov, iovcnt);
		if (wret >= left)
			break;
		if (wret > 0) {
			left -= wret;
			if (iov != localiov) {
				(void)memcpy(localiov, iov,
				    iovcnt * sizeof(struct iovec));
				iov = localiov;
			}
			for (cnt = 0; wret >= iov->iov_len; ++cnt) {
				wret -= iov->iov_len;
				++iov;
				--iovcnt;
			}
			if (wret) {
				iov->iov_base =
				    (char *)iov->iov_base + wret;
				iov->iov_len -= wret;
			}
			continue;
		} else if (wret == 0) {
			(void)snprintf(errbuf, sizeof(errbuf),
			    "%s: failed writing %zu bytes to `%s'", __func__,
			    left, device);
			(void) close(fd);
			if (forked)
				_exit(1);
			return errbuf;
		}
		if (errno == EWOULDBLOCK) {
			pid_t cpid;

			if (forked) {
				(void)close(fd);
				_exit(1);
			}
			cpid = fork();
			if (cpid < 0) {
				(void)snprintf(errbuf, sizeof(errbuf),
				    "%s: Cannot fork (%s)", __func__,
				    strerror(errno));
				(void)close(fd);
				return errbuf;
			}
			if (cpid) {	/* parent */
				(void)close(fd);
				return NULL;
			}
			forked++;
			/* wait at most tmout seconds */
			(void)signal(SIGALRM, SIG_DFL);
			(void)signal(SIGTERM, SIG_DFL); /* XXX */
			sigfillset(&nset);
			(void)sigprocmask(SIG_UNBLOCK, &nset, NULL);
			(void)alarm((u_int)tmout);
			(void)fcntl(fd, F_SETFL, 0);	/* clear O_NONBLOCK */
			continue;
		}
		/*
		 * We get ENODEV on a slip line if we're running as root,
		 * and EIO if the line just went away.
		 */
		if (errno == ENODEV || errno == EIO)
			break;
		(void) close(fd);
		if (forked)
			_exit(1);
		(void)snprintf(errbuf, sizeof(errbuf),
		    "%s: Write to line `%s' failed (%s)", __func__,
		    device, strerror(errno));
		return errbuf;
	}

	(void) close(fd);
	if (forked)
		_exit(0);
	return NULL;
}
