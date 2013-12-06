/*	$NetBSD: pty.c,v 1.31 2009/02/20 16:44:06 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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
static char sccsid[] = "@(#)pty.c	8.3 (Berkeley) 5/16/94";
#else
__RCSID("$NetBSD: pty.c,v 1.31 2009/02/20 16:44:06 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

/*
 * XXX: `v' removed until no ports are using console devices which use ttyv0
 */
#define TTY_LETTERS	"pqrstuwxyzPQRST"
#define TTY_OLD_SUFFIX	"0123456789abcdef"
#define TTY_NEW_SUFFIX	"ghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

int
openpty(int *amaster, int *aslave, char *name, struct termios *term,
	struct winsize *winp)
{
	char line[] = "/dev/XtyXX";
	const char *cp1, *cp2, *cp, *linep;
	int master, slave;
	gid_t ttygid;
	mode_t mode;
	struct group grs, *grp;
	char grbuf[1024];

	_DIAGASSERT(amaster != NULL);
	_DIAGASSERT(aslave != NULL);
	/* name may be NULL */
	/* term may be NULL */
	/* winp may be NULL */

#if !defined(__minix)
	if ((master = open("/dev/ptm", O_RDWR)) != -1) {
		struct ptmget pt;
		if (ioctl(master, TIOCPTMGET, &pt) != -1) {
			(void)close(master);
			master = pt.cfd;
			slave = pt.sfd;
			linep = pt.sn;
			goto gotit;
		}
		(void)close(master);
	}
#endif /* !defined(__minix) */

	(void)getgrnam_r("tty", &grs, grbuf, sizeof(grbuf), &grp);
	if (grp != NULL) {
		ttygid = grp->gr_gid;
		mode = S_IRUSR|S_IWUSR|S_IWGRP;
	} else {
		ttygid = getgid();
		mode = S_IRUSR|S_IWUSR;
	}

	for (cp1 = TTY_LETTERS; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp = cp2 = TTY_OLD_SUFFIX TTY_NEW_SUFFIX; *cp2; cp2++) {
			line[5] = 'p';
			line[9] = *cp2;
#ifdef __minix
			if ((master = open(line, O_RDWR | O_NOCTTY, 0)) == -1) {
#else
			if ((master = open(line, O_RDWR, 0)) == -1) {
#endif
				if (errno != ENOENT)
					continue;	/* busy */
				if ((size_t)(cp2 - cp + 1) < sizeof(TTY_OLD_SUFFIX))
					return -1; /* out of ptys */
				else	
					break;	/* out of ptys in this group */
			}
			line[5] = 't';
			linep = line;
			if (chown(line, getuid(), ttygid) == 0 &&
			    chmod(line, mode) == 0 &&
#if !defined(__minix)
			    revoke(line) == 0 &&
#endif /* !defined(__minix) */
#ifdef __minix
			(slave = open(line, O_RDWR | O_NOCTTY, 0)) != -1) {
#else
			    (slave = open(line, O_RDWR, 0)) != -1) {
#endif
#if !defined(__minix)
gotit:
#endif /* !defined(__minix) */
				*amaster = master;
				*aslave = slave;
				if (name)
					(void)strcpy(name, linep);
				if (term)
					(void)tcsetattr(slave, TCSAFLUSH, term);
				if (winp)
					(void)ioctl(slave, TIOCSWINSZ, winp);
				return 0;
			}
			(void)close(master);
		}
	}
	errno = ENOENT;	/* out of ptys */
	return -1;
}

pid_t
forkpty(int *amaster, char *name, struct termios *term, struct winsize *winp)
{
	int master, slave;
	pid_t pid;

	_DIAGASSERT(amaster != NULL);
	/* name may be NULL */
	/* term may be NULL */
	/* winp may be NULL */

	if (openpty(&master, &slave, name, term, winp) == -1)
		return -1;
	switch (pid = fork()) {
	case -1:
		return -1;
	case 0:
		/*
		 * child
		 */
		(void)close(master);
		login_tty(slave);
		return 0;
	}
	/*
	 * parent
	 */
	*amaster = master;
	(void)close(slave);
	return pid;
}
