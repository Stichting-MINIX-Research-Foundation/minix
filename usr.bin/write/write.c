/*	$NetBSD: write.c,v 1.27 2011/09/06 18:46:35 joerg Exp $	*/

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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)write.c	8.2 (Berkeley) 4/27/95";
#else
__RCSID("$NetBSD: write.c,v 1.27 2011/09/06 18:46:35 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "utmpentry.h"
#include "term_chk.h"

__dead static void done(int);
static void do_write(int, const char *, const uid_t);
static void wr_fputs(char *);
static int search_utmp(char *, char *, uid_t, gid_t);
static int utmp_chk(const char *, const char *);

int
main(int argc, char **argv)
{
	time_t atime;
	uid_t myuid, uid;
	int msgsok, ttyfd;
	char *mytty;
	gid_t saved_egid = getegid();

	if (setegid(getgid()) == -1)
		err(1, "setegid");
	myuid = getuid();
	ttyfd = -1;

	mytty = check_sender(&atime, myuid, saved_egid);

	/* check args */
	switch (argc) {
	case 2:
		ttyfd = search_utmp(argv[1], mytty, myuid, saved_egid);
		break;
	case 3:
		if (!strncmp(argv[2], _PATH_DEV, strlen(_PATH_DEV)))
			argv[2] += strlen(_PATH_DEV);
		if (uid_from_user(argv[1], &uid) == -1)
			errx(1, "%s: unknown user", argv[1]);
		if (utmp_chk(argv[1], argv[2]))
			errx(1, "%s is not logged in on %s",
			    argv[1], argv[2]);
		ttyfd = term_chk(uid, argv[2], &msgsok, &atime, 0, saved_egid);
		if (ttyfd == -1)
			err(1, "%s%s", _PATH_DEV, argv[2]);
		if (myuid && !msgsok)
			errx(1, "%s has messages disabled on %s",
			    argv[1], argv[2]);
		break;
	default:
		(void)fprintf(stderr, "usage: write user [tty]\n");
		exit(1);
	}
	if (setgid(getgid()) == -1)
		err(1, "setgid");
	do_write(ttyfd, mytty, myuid);
	done(0);
	/* NOTREACHED */
#ifdef __GNUC__
	return (0);
#endif
}

/*
 * utmp_chk - checks that the given user is actually logged in on
 *     the given tty
 */
static int
utmp_chk(const char *user, const char *tty)
{
	struct utmpentry *ep;

	(void)getutentries(NULL, &ep);

	for (; ep; ep = ep->next)
		if (strcmp(user, ep->name) == 0 && strcmp(tty, ep->line) == 0)
			return(0);
	return(1);
}

/*
 * search_utmp - search utmp for the "best" terminal to write to
 *
 * Ignores terminals with messages disabled, and of the rest, returns
 * the one with the most recent access time.  Returns as value the number
 * of the user's terminals with messages enabled, or -1 if the user is
 * not logged in at all.
 *
 * Special case for writing to yourself - ignore the terminal you're
 * writing from, unless that's the only terminal with messages enabled.
 */
static int
search_utmp(char *user, char *mytty, uid_t myuid, gid_t saved_egid)
{
	char tty[MAXPATHLEN];
	time_t bestatime, atime;
	int nloggedttys, nttys, msgsok, user_is_me;
	struct utmpentry *ep;
	int fd, nfd;
	uid_t uid;

	if (uid_from_user(user, &uid) == -1)
		errx(1, "%s: unknown user", user);

	(void)getutentries(NULL, &ep);

	nloggedttys = nttys = 0;
	bestatime = 0;
	user_is_me = 0;
	fd = -1;
	for (; ep; ep = ep->next)
		if (strcmp(user, ep->name) == 0) {
			++nloggedttys;
			nfd = term_chk(uid, ep->line, &msgsok, &atime, 0,
			    saved_egid);
			if (nfd == -1)
				continue;	/* bad term? skip */
			if (myuid && !msgsok) {
				close(nfd);
				continue;	/* skip ttys with msgs off */
			}
			if (strcmp(ep->line, mytty) == 0) {
				user_is_me = 1;
				if (fd == -1)
					fd = nfd;
				else
					close(nfd);
				continue;	/* don't write to yourself */
			}
			++nttys;
			if (atime > bestatime) {
				bestatime = atime;
				(void)strlcpy(tty, ep->line, sizeof(tty));
				close(fd);
				fd = nfd;
			} else
				close(nfd);
		}

	if (nloggedttys == 0)
		errx(1, "%s is not logged in", user);
	if (nttys == 0) {
		if (user_is_me)			/* ok, so write to yourself! */
			return fd;
		errx(1, "%s has messages disabled", user);
	} else if (nttys > 1)
		warnx("%s is logged in more than once; writing to %s",
		    user, tty);
	return fd;
}

/*
 * do_write - actually make the connection
 */
static void
do_write(int ttyfd, const char *mytty, const uid_t myuid)
{
	const char *login;
	char *nows;
	struct passwd *pwd;
	time_t now;
	char host[MAXHOSTNAMELEN + 1], line[512];

	/* Determine our login name before we re-open stdout */
	if ((login = getlogin()) == NULL) {
		if ((pwd = getpwuid(myuid)) != NULL)
			login = pwd->pw_name;
		else	login = "???";
	}

	if (dup2(ttyfd, STDOUT_FILENO) == -1)
		err(1, "dup2");

	(void)signal(SIGINT, done);
	(void)signal(SIGHUP, done);
	(void)close(ttyfd);

	/* print greeting */
	if (gethostname(host, sizeof(host)) < 0)
		(void)strlcpy(host, "???", sizeof(host));
	else
		host[sizeof(host) - 1] = '\0';
	now = time(NULL);
	nows = ctime(&now);
	nows[16] = '\0';
	(void)printf("\r\n\a\a\aMessage from %s@%s on %s at %s ...\r\n",
	    login, host, mytty, nows + 11);

	while (fgets(line, sizeof(line), stdin) != NULL)
		wr_fputs(line);
}

/*
 * done - cleanup and exit
 */
static void
done(int signo)
{

	(void)write(STDOUT_FILENO, "EOF\r\n", sizeof("EOF\r\n") - 1);
	if (signo == 0)
		exit(0);
	else
		_exit(0);
}

/*
 * wr_fputs - like fputs(), but makes control characters visible and
 *     turns \n into \r\n
 */
static void
wr_fputs(char *s)
{
	unsigned char c;

#define	PUTC(c)	if (putchar(c) == EOF) goto err;

	for (; *s != '\0'; ++s) {
		c = toascii(*s);
		if (c == '\n') {
			PUTC('\r');
		} else if (!isprint(c) && !isspace(c) && c != '\a') {
			PUTC('^');
			c ^= 0x40;	/* DEL to ?, others to alpha */
		}
		PUTC(c);
	}
	return;

err:	err(1, NULL);
#undef PUTC
}
