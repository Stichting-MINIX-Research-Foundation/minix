/*	$NetBSD: fsutil.c,v 1.26 2015/06/21 04:01:40 dholland Exp $	*/

/*
 * Copyright (c) 1990, 1993
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
#ifndef lint
__RCSID("$NetBSD: fsutil.c,v 1.26 2015/06/21 04:01:40 dholland Exp $");
#endif /* not lint */

/*
 * used by sbin/fsck
 * used by sbin/fsck_ext2fs
 * used by sbin/fsck_ffs
 * used by sbin/fsck_lfs
 * used by sbin/fsck_msdos
 * used by sbin/fsck_v7fs
 * used by sbin/fsdb
 * used by usr.sbin/quotacheck
 */

#include <sys/param.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <fstab.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <util.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "fsutil.h"
#include "exitvalues.h"

volatile sig_atomic_t returntosingle;

static const char *dev = NULL;
static int hot = 0;
static int preen = 0;
int quiet;
#define F_ERROR	0x80000000

void
setcdevname(const char *cd, int pr)
{

	dev = cd;
	preen = pr;
}

const char *
cdevname(void)
{

	return dev;
}

int
hotroot(void)
{

	return hot;
}

/*VARARGS*/
void
errexit(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(FSCK_EXIT_CHECK_FAILED);
}

void
vmsg(int fatal, const char *fmt, va_list ap)
{
	int serr = fatal & F_ERROR;
	int serrno = errno;
	fatal &= ~F_ERROR;

	if (!fatal && preen)
		(void)printf("%s: ", dev);
	if (quiet && !preen) {
		(void)printf("** %s (vmsg)\n", dev);
		quiet = 0;
	}

	(void) vprintf(fmt, ap);
	if (serr) 
		printf(" (%s)", strerror(serrno));

	if (fatal && preen)
		(void) printf("\n");

	if (fatal && preen) {
		(void) printf(
		    "%s: UNEXPECTED INCONSISTENCY; RUN %s MANUALLY.\n",
		    dev, getprogname());
		exit(FSCK_EXIT_CHECK_FAILED);
	}
}

/*VARARGS*/
void
pfatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(1, fmt, ap);
	va_end(ap);
}

/*VARARGS*/
void
pwarn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(0, fmt, ap);
	va_end(ap);
}

void
perr(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(1 | F_ERROR, fmt, ap);
	va_end(ap);
}

void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(1, fmt, ap);
	va_end(ap);
	exit(FSCK_EXIT_CHECK_FAILED);
}

const char *
blockcheck(const char *origname)
{
#if defined(__minix)
	return origname;
#else
	struct stat stslash, stblock, stchar;
	const char *newname, *raw, *cooked;
	struct fstab *fsp;
	int retried = 0;
	ssize_t len;
	char cbuf[MAXPATHLEN];
	static char buf[MAXPATHLEN];

	hot = 0;
	if (stat("/", &stslash) < 0) {
		perr("Can't stat `/'");
		return (origname);
	}
	len = readlink(origname, cbuf, sizeof(cbuf)-1);
	if (len == -1) {
		newname = origname;
	} else {
		cbuf[len] = '\0';
		newname = cbuf;
	}
retry:
	if (stat(newname, &stblock) < 0) {
		perr("Can't stat `%s'", newname);
		return origname;
	}
	if (S_ISBLK(stblock.st_mode)) {
		if (stslash.st_dev == stblock.st_rdev)
			hot++;
		raw = getdiskrawname(buf, sizeof(buf), newname);
		if (raw == NULL) {
			perr("Can't convert to raw `%s'", newname);
			return origname;
		}
		if (stat(raw, &stchar) < 0) {
			perr("Can't stat `%s'", raw);
			return origname;
		}
		if (S_ISCHR(stchar.st_mode)) {
			return raw;
		} else {
			perr("%s is not a character device\n", raw);
			return origname;
		}
	} else if (S_ISCHR(stblock.st_mode) && !retried) {
		cooked = getdiskcookedname(cbuf, sizeof(cbuf), newname);
		if (cooked == NULL) {
			perr("Can't convert to cooked `%s'", newname);
			return origname;
		} else
			newname = cooked;
		retried++;
		goto retry;
	} else if ((fsp = getfsfile(newname)) != 0 && !retried) {
		newname = getfsspecname(cbuf, sizeof(cbuf), fsp->fs_spec);
		if (newname == NULL)
			perr("%s", buf);
		retried++;
		goto retry;
	}
	/*
	 * Not a block or character device, just return name and
	 * let the user decide whether to use it.
	 */
	return origname;
#endif /* defined(__minix) */
}

const char *
print_mtime(time_t t)
{
	static char b[128];
	char *p = ctime(&t);
	if (p != NULL)
		(void)snprintf(b, sizeof(b), "%12.12s %4.4s ", &p[4], &p[20]);
	else
		(void)snprintf(b, sizeof(b), "%lld ", (long long)t);
	return b;
}


void
catch(int n)
{
	if (ckfinish) (*ckfinish)(0);
	_exit(FSCK_EXIT_SIGNALLED);
}

/*
 * When preening, allow a single quit to signal
 * a special exit after filesystem checks complete
 * so that reboot sequence may be interrupted.
 */
void
catchquit(int n)
{
	static const char msg[] =
	    "returning to single-user after filesystem check\n";
	int serrno = errno;

	(void)write(STDOUT_FILENO, msg, sizeof(msg) - 1);
	returntosingle = 1;
	(void)signal(SIGQUIT, SIG_DFL);
	errno = serrno;
}

/*
 * Ignore a single quit signal; wait and flush just in case.
 * Used by child processes in preen.
 */
void
voidquit(int n)
{
	int serrno = errno;

	sleep(1);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_DFL);
	errno = serrno;
}
