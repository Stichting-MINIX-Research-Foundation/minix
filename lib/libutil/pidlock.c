/*	$NetBSD: pidlock.c,v 1.16 2012/04/07 16:17:17 christos Exp $ */

/*
 * Copyright 1996, 1997 by Curt Sampson <cjs@NetBSD.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
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
__RCSID("$NetBSD: pidlock.c,v 1.16 2012/04/07 16:17:17 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <paths.h>

/*
 * Create a lockfile. Return 0 when locked, -1 on error.
 */
int
pidlock(const char *lockfile, int flags, pid_t *locker, const char *info)
{
	char	tempfile[MAXPATHLEN];
	char	hostname[MAXHOSTNAMELEN + 1];
	pid_t	pid2 = -1;
	struct	stat st;
	ssize_t	n;
	int	f = -1, savee;
	char	s[256];
	char	*p;
	size_t	len;

	_DIAGASSERT(lockfile != NULL);
	/* locker may be NULL */
	/* info may be NULL */


	if (gethostname(hostname, sizeof(hostname)))
		return -1;
	hostname[sizeof(hostname) - 1] = '\0';

	/*
	 * Build a path to the temporary file.
	 * We use the path with the PID and hostname appended.
	 * XXX This is not thread safe.
	 */
	if (snprintf(tempfile, sizeof(tempfile), "%s.%d.%s", lockfile,
	    (int) getpid(), hostname) >= (int)sizeof(tempfile))  {
		errno = ENAMETOOLONG;
		return -1;
	}

	/* Open it, write pid, hostname, info. */
	if ((f = open(tempfile, O_WRONLY|O_CREAT|O_TRUNC, 0600)) == -1)
		goto out;

	(void)snprintf(s, sizeof(s), "%10d\n", getpid());	/* pid */
	if (write(f, s, (size_t)11) != 11)
		goto out;

	if ((flags & PIDLOCK_USEHOSTNAME))  {		/* hostname */
		len = strlen(hostname);
		if ((size_t)write(f, hostname, len) != len
		    || write(f, "\n", (size_t)1) != 1)
			goto out;
	}
	if (info)  {					/* info */
		if (!(flags & PIDLOCK_USEHOSTNAME))  {
			/* write blank line because there's no hostname */
			if (write(f, "\n", (size_t)1) != 1)
				goto out;
		}
		len = strlen(info);
		if ((size_t)write(f, info, len) != len ||
		    write(f, "\n", (size_t)1) != 1)
			goto out;
	}
	(void)close(f);
	f = -1;

	/* Hard link the temporary file to the real lock file. */
	/* This is an atomic operation. */
lockfailed:
	while (link(tempfile, lockfile) == -1)  {
		if (errno != EEXIST)
			goto out;
		/* Find out who has this lockfile. */
		if ((f = open(lockfile, O_RDONLY, 0)) != -1)  {
			if ((n = read(f, s, (size_t)11)) == -1)
				goto out;
			if (n == 0) {
				errno = EINVAL;
				goto out;
			}
			pid2 = atoi(s);
			if ((n = read(f, s, sizeof(s) - 2)) == -1)
				goto out;
			if (n == 0)
				*s = '\0';
			s[sizeof(s) - 1] = '\0';
			if ((p = strchr(s, '\n')) != NULL)
				*p = '\0';
			(void)close(f);
			f = -1;

			if ((flags & PIDLOCK_USEHOSTNAME) == 0 ||
			    strcmp(s, hostname) == 0)  {
				if (kill(pid2, 0) == -1 && errno == ESRCH)  {
					/* process doesn't exist */
					(void)unlink(lockfile);
					continue;
				}
			}
		}
		if (flags & PIDLOCK_NONBLOCK)  {
			if (locker)
				*locker = pid2;
			errno = EWOULDBLOCK;
			goto out;
		} else
			sleep(5);
	}
	/*
	 * Check to see that we really were successful (in case we're
	 * using NFS) by making sure that something really is linked
	 * to our tempfile (reference count is two).
	 */
	if (stat(tempfile, &st) == -1)
		goto out;
	if (st.st_nlink != 2)
		goto lockfailed;

	(void)unlink(tempfile);
 	if (locker)
 		*locker = getpid();	/* return this process's PID on lock */
	errno = 0;
	return 0;
out:
	savee = errno;
	if (f != -1)
		(void)close(f);
	(void)unlink(tempfile);
	errno = savee;
	return -1;
}

static int
checktty(const char *tty)
{
	char	ttyfile[MAXPATHLEN];
	struct stat sb;

	(void)strlcpy(ttyfile, _PATH_DEV, sizeof(ttyfile));
	(void)strlcat(ttyfile, tty, sizeof(ttyfile));

	/* make sure the tty exists */
	if (stat(ttyfile, &sb) == -1)
		return -1;
	if (!S_ISCHR(sb.st_mode))  {
		errno = EFTYPE;
		return -1;
	}
	return 0;
}

#define LOCKPATH	"/var/spool/lock/LCK.."

static char *
makelock(char *buf, size_t bufsiz, const char *tty)
{
	(void)strlcpy(buf, LOCKPATH, bufsiz);
	(void)strlcat(buf, tty, bufsiz);
	return buf;
}

/*ARGSUSED*/
int
ttylock(const char *tty, int flags, pid_t *locker)
{
	char	lockfile[MAXPATHLEN];

	_DIAGASSERT(tty != NULL);

	if (checktty(tty) != 0)
		return -1;

	/* do the lock */
	return pidlock(makelock(lockfile, sizeof(lockfile), tty),
	    flags, locker, 0);
}

int
ttyunlock(const char *tty)
{
	char	lockfile[MAXPATHLEN];

	_DIAGASSERT(tty != NULL);

	if (checktty(tty) != 0)
		return -1;

	/* remove the lock */
	return unlink(makelock(lockfile, sizeof(lockfile), tty));
}
