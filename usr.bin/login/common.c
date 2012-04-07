/*	$NetBSD: common.c,v 1.3 2009/12/29 20:15:15 christos Exp $	*/

/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
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
__RCSID("$NetBSD: common.c,v 1.3 2009/12/29 20:15:15 christos Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <ttyent.h>
#include <setjmp.h>
#include <time.h>
#include <pwd.h>
#include <err.h>
#include <vis.h>
#include <util.h>

#include "pathnames.h"
#include "common.h"

#if defined(KERBEROS5)
#define	NBUFSIZ		(MAXLOGNAME + 1 + 5)	/* .root suffix */
#else
#define	NBUFSIZ		(MAXLOGNAME + 1)
#endif

#ifdef SUPPORT_UTMP
#include <utmp.h>
static void	 doutmp(void);
static void	 dolastlog(int);
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
static void	 doutmpx(void);
static void	 dolastlogx(int);
#endif

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
u_int	timeout = 300;

void	 decode_ss(const char *);
struct	passwd *pwd;
int	failures, have_ss;
char	term[64], *envinit[1], *hostname, *username, *tty, *nested;
struct timeval now;
struct sockaddr_storage ss;

void
getloginname(void)
{
	int ch;
	char *p;
	static char nbuf[NBUFSIZ];

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				exit(EXIT_FAILURE);
			}
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf) {
			if (nbuf[0] == '-')
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
		}
	}
}

int
rootterm(char *ttyn)
{
	struct ttyent *t;

	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}

static jmp_buf motdinterrupt;

void
motd(char *fname)
{
	int fd, nchars;
	sig_t oldint;
	char tbuf[8192];

	if ((fd = open(fname ? fname : _PATH_MOTDFILE, O_RDONLY, 0)) < 0)
		return;
	oldint = signal(SIGINT, sigint);
	if (setjmp(motdinterrupt) == 0)
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
	(void)signal(SIGINT, oldint);
	(void)close(fd);
}

/* ARGSUSED */
void
sigint(int signo)
{

	longjmp(motdinterrupt, 1);
}

/* ARGSUSED */
void
timedout(int signo)
{

	(void)fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	exit(EXIT_FAILURE);
}

void
update_db(int quietlog, int rootlogin, int fflag)
{
	struct sockaddr_storage ass;
	char assbuf[1024];
	socklen_t alen;
	const char *hname;
	int remote;

	hname = (hostname == NULL) ? "?" : hostname;
	if (getpeername(STDIN_FILENO, (struct sockaddr *)&ass, &alen) != -1) {
		(void)sockaddr_snprintf(assbuf,
		    sizeof(assbuf), "%A (%a)", (void *)&ass);
		if (have_ss) {
			char ssbuf[1024];
			(void)sockaddr_snprintf(ssbuf,
			    sizeof(ssbuf), "%A(%a)", (void *)&ss);
			 if (memcmp(&ass, &ss, alen) != 0)
				syslog(LOG_NOTICE,
				    "login %s on tty %s address mismatch "
				    "passed %s != actual %s", username, tty,
				    ssbuf, assbuf);
		} else
			ss = ass;
		remote = 1;
	} else if (have_ss) {
		(void)sockaddr_snprintf(assbuf,
		    sizeof(assbuf), "%A(%a)", (void *)&ss);
		remote = 1;
	} else if (hostname) {
		(void)snprintf(assbuf, sizeof(assbuf), "? ?");
		remote = 1;
	} else
		remote = 0;

	/* If fflag is on, assume caller/authenticator has logged root login. */
	if (rootlogin && fflag == 0) {
		if (remote)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) on tty %s from %s /"
			    " %s", username, tty, hname, assbuf);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) on tty %s",
			    username, tty);
	} else if (nested != NULL) {
		if (remote)
			syslog(LOG_NOTICE, "%s to %s on tty %s from %s / "
			    "%s", nested, pwd->pw_name, tty, hname, assbuf);
		else
			syslog(LOG_NOTICE, "%s to %s on tty %s", nested,
			    pwd->pw_name, tty);
	} else {
		if (remote)
			syslog(LOG_NOTICE, "%s on tty %s from %s / %s",
			    pwd->pw_name, tty, hname, assbuf);
		else
			syslog(LOG_NOTICE, "%s on tty %s", 
			    pwd->pw_name, tty);
	}
	(void)gettimeofday(&now, NULL);
#ifdef SUPPORT_UTMPX
	doutmpx();
	dolastlogx(quietlog);
	quietlog = 1;
#endif	
#ifdef SUPPORT_UTMP
	doutmp();
	dolastlog(quietlog);
#endif
}

#ifdef SUPPORT_UTMPX
static void
doutmpx(void)
{
	struct utmpx utmpx;
	char *t;

	memset((void *)&utmpx, 0, sizeof(utmpx));
	utmpx.ut_tv = now;
	(void)strncpy(utmpx.ut_name, username, sizeof(utmpx.ut_name));
	if (hostname) {
		(void)strncpy(utmpx.ut_host, hostname, sizeof(utmpx.ut_host));
		utmpx.ut_ss = ss;
	}
	(void)strncpy(utmpx.ut_line, tty, sizeof(utmpx.ut_line));
	utmpx.ut_type = USER_PROCESS;
	utmpx.ut_pid = getpid();
	t = tty + strlen(tty);
	if (t - tty >= sizeof(utmpx.ut_id)) {
	    (void)strncpy(utmpx.ut_id, t - sizeof(utmpx.ut_id),
		sizeof(utmpx.ut_id));
	} else {
	    (void)strncpy(utmpx.ut_id, tty, sizeof(utmpx.ut_id));
	}
	if (pututxline(&utmpx) == NULL)
		syslog(LOG_NOTICE, "Cannot update utmpx: %m");
	endutxent();
	if (updwtmpx(_PATH_WTMPX, &utmpx) != 0)
		syslog(LOG_NOTICE, "Cannot update wtmpx: %m");
}

static void
dolastlogx(int quiet)
{
	struct lastlogx ll;
	if (!quiet && getlastlogx(_PATH_LASTLOGX, pwd->pw_uid, &ll) != NULL) {
		time_t t = (time_t)ll.ll_tv.tv_sec;
		(void)printf("Last login: %.24s ", ctime(&t));
		if (*ll.ll_host != '\0')
			(void)printf("from %.*s ",
			    (int)sizeof(ll.ll_host),
			    ll.ll_host);
		(void)printf("on %.*s\n",
		    (int)sizeof(ll.ll_line),
		    ll.ll_line);
	}
	ll.ll_tv = now;
	(void)strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
	if (hostname)
		(void)strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
	else
		(void)memset(ll.ll_host, '\0', sizeof(ll.ll_host));
	if (have_ss)
		ll.ll_ss = ss;
	else
		(void)memset(&ll.ll_ss, 0, sizeof(ll.ll_ss));
	if (updlastlogx(_PATH_LASTLOGX, pwd->pw_uid, &ll) != 0)
		syslog(LOG_NOTICE, "Cannot update lastlogx: %m");
}
#endif

#ifdef SUPPORT_UTMP
static void
doutmp(void)
{
	struct utmp utmp;

	(void)memset((void *)&utmp, 0, sizeof(utmp));
	utmp.ut_time = now.tv_sec;
	(void)strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
	if (hostname)
		(void)strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
	(void)strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
	login(&utmp);
}

static void
dolastlog(int quiet)
{
	struct lastlog ll;
	int fd;

	if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)(pwd->pw_uid * sizeof(ll)), SEEK_SET);
		if (!quiet) {
			if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				(void)printf("Last login: %.24s ",
				    ctime(&ll.ll_time));
				if (*ll.ll_host != '\0')
					(void)printf("from %.*s ",
					    (int)sizeof(ll.ll_host),
					    ll.ll_host);
				(void)printf("on %.*s\n",
				    (int)sizeof(ll.ll_line), ll.ll_line);
			}
			(void)lseek(fd, (off_t)(pwd->pw_uid * sizeof(ll)),
			    SEEK_SET);
		}
		memset((void *)&ll, 0, sizeof(ll));
		ll.ll_time = now.tv_sec;
		(void)strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		if (hostname)
			(void)strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	}
}
#endif

void
badlogin(const char *name)
{

	if (failures == 0)
		return;
	if (hostname) {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s",
		    failures, failures > 1 ? "S" : "", hostname);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s FROM %s, %s",
		    failures, failures > 1 ? "S" : "", hostname, name);
	} else {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s",
		    failures, failures > 1 ? "S" : "", tty);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
	}
}

const char *
stypeof(const char *ttyid)
{
	struct ttyent *t;

	return (ttyid && (t = getttynam(ttyid)) ? t->ty_type : NULL);
}

void
sleepexit(int eval)
{

	(void)sleep(5);
	exit(eval);
}

void
decode_ss(const char *arg)
{
	struct sockaddr_storage *ssp;
	size_t len = strlen(arg);
	
	if (len > sizeof(*ssp) * 4 + 1 || len < sizeof(*ssp))
		errx(EXIT_FAILURE, "Bad argument");

	if ((ssp = malloc(len)) == NULL)
		err(EXIT_FAILURE, NULL);

	if (strunvis((char *)ssp, arg) != sizeof(*ssp))
		errx(EXIT_FAILURE, "Decoding error");

	(void)memcpy(&ss, ssp, sizeof(ss));
	free(ssp);
	have_ss = 1;
}
