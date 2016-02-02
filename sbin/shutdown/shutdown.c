/*	$NetBSD: shutdown.c,v 1.56 2014/03/28 18:27:14 apb Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1990, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)shutdown.c	8.4 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: shutdown.c,v 1.56 2014/03/28 18:27:14 apb Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syslog.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <errno.h>

#include "pathnames.h"

#ifdef DEBUG
#undef _PATH_NOLOGIN
#define	_PATH_NOLOGIN	"./nologin"
#undef _PATH_FASTBOOT
#define	_PATH_FASTBOOT	"./fastboot"
#endif

#define	H		*60*60
#define	M		*60
#define	S		*1
#define	NOLOG_TIME	5*60
static const struct interval {
	time_t timeleft, timetowait;
} tlist[] = {
	{ 10 H,  5 H },	{  5 H,  3 H },	{  2 H,  1 H },	{ 1 H, 30 M },
	{ 30 M, 10 M },	{ 20 M, 10 M },	{ 10 M,  5 M },	{ 5 M,  3 M },
	{  2 M,  1 M },	{  1 M, 30 S },	{ 30 S, 30 S },
	{  0, 0 }
};
#undef H
#undef M
#undef S

static time_t offset, shuttime;
static int dofast, dohalt, doreboot, killflg, nofork, nosync, dodump;
static size_t mbuflen;
static int dopowerdown;
static int dodebug, dosilent, doverbose;
static const char *whom;
static char mbuf[BUFSIZ];
static char *bootstr;

static void badtime(void) __dead;
static void die_you_gravy_sucking_pig_dog(void) __dead;
static void doitfast(void);
static void dorcshutdown(void);
static void finish(int) __dead;
static void getoffset(char *);
static void loop(void) __dead;
static void nolog(void);
static void timeout(int) __dead;
static void timewarn(time_t);
static void usage(void) __dead;

int
main(int argc, char *argv[])
{
	char *p, *endp;
	struct passwd *pw;
	size_t arglen, len;
	int ch;

	(void)setprogname(argv[0]);
#ifndef DEBUG
	if (geteuid())
		errx(1, "%s: Not super-user", strerror(EPERM));
#endif
	while ((ch = getopt(argc, argv, "b:Ddfhknprvxz")) != -1)
		switch (ch) {
		case 'b':
			bootstr = optarg;
			break;
		case 'd':
			dodump = 1;
			break;
		case 'D':
			nofork = 1;
			break;
		case 'f':
			dofast = 1;
			break;
		case 'p':
			dopowerdown = 1;
			/* FALLTHROUGH */
		case 'h':
			dohalt = 1;
			break;
		case 'k':
			killflg = 1;
			break;
		case 'n':
			nosync = 1;
			break;
		case 'r':
			doreboot = 1;
			break;
		case 'v':
			doverbose = 1;
			break;
		case 'x':
			dodebug = 1;
			break;
		case 'z':
			dosilent = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (dodump && !dohalt && !doreboot)
		doreboot = 1;

	if (dofast && nosync) {
		warnx("Incompatible options -f and -n");
		usage();
	}
	if (dohalt && doreboot) {
		const char *which_flag = dopowerdown ? "p" : "h";

		warnx("Incompatible options -%s and -r", which_flag);
		usage();
	}

	getoffset(*argv++);

	if (argv[0]) {
		if (strcmp(argv[0], "-") || argv[1]) {
			for (p = mbuf, len = sizeof(mbuf); *argv; ++argv) {
				arglen = strlen(*argv);
				if ((len -= arglen) <= 2)
					break;
				if (p != mbuf)
					*p++ = ' ';
				(void)memmove(p, *argv, arglen);
				p += arglen;
			}
			*p = '\n';
			*++p = '\0';
		} else {
			p = mbuf;
			endp = mbuf + sizeof(mbuf) - 2;
			for (;;) {
				if (!fgets(p, endp - p + 1, stdin))
					break;
				for (; *p &&  p < endp; ++p);
				if (p == endp) {
					*p = '\n';
					*++p = '\0';
					break;
				}
			}
		}
	}
	mbuflen = strlen(mbuf);

	if (offset)
		(void)printf("Shutdown at %.24s.\n", ctime(&shuttime));
	else
		(void)printf("Shutdown NOW!\n");

	if (!(whom = getlogin()))
		whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";

#ifdef DEBUG
	(void)putc('\n', stdout);
#else
	(void)setpriority(PRIO_PROCESS, 0, PRIO_MIN);
	if (nofork == 0) {
		int forkpid;

		forkpid = fork();
		if (forkpid == -1) {
			perror("shutdown: fork");
			exit(1);
		}
		if (forkpid) {
			(void)printf("shutdown: [pid %d]\n", forkpid);
			exit(0);
		}
		(void)setsid();
	}
#endif
	openlog("shutdown", LOG_CONS, LOG_AUTH);
	loop();
	/* NOTREACHED */
#ifdef __GNUC__
	return 1;
#endif
}

static void
loop(void)
{
	const struct interval *tp;
	u_int sltime;
	int logged;

	if (offset <= NOLOG_TIME) {
		logged = 1;
		nolog();
	}
	else
		logged = 0;
	tp = tlist;
	if (tp->timeleft < offset)
		(void)sleep((u_int)(offset - tp->timeleft));
	else {
		while (offset < tp->timeleft)
			++tp;
		/*
		 * Warn now, if going to sleep more than a fifth of
		 * the next wait time.
		 */
		if ((sltime = offset - tp->timeleft) != 0) {
			if (sltime > tp->timetowait / 5)
				timewarn(offset);
			(void)sleep(sltime);
		}
	}
	for (;; ++tp) {
		timewarn(tp->timeleft);
		if (!logged && tp->timeleft <= NOLOG_TIME) {
			logged = 1;
			nolog();
		}
		(void)sleep((u_int)tp->timetowait);
		if (!tp->timeleft)
			break;
	}
	die_you_gravy_sucking_pig_dog();
}

static jmp_buf alarmbuf;

static void
timewarn(time_t timeleft)
{
	static int first;
	static char hostname[MAXHOSTNAMELEN + 1];
	FILE *pf;
	char wcmd[MAXPATHLEN + 4];

	if (!first++) {
		(void)gethostname(hostname, sizeof(hostname));
		hostname[sizeof(hostname) - 1] = '\0';
	}

	/* undoc -n option to wall suppresses normal wall banner */
	(void)snprintf(wcmd, sizeof wcmd, "%s -n", _PATH_WALL);
	if ((pf = popen(wcmd, "w")) == NULL) {
		syslog(LOG_ERR, "%s: Can't find `%s' (%m)", getprogname(),
		    _PATH_WALL);
		return;
	}

	(void)fprintf(pf,
	    "\007*** %sSystem shutdown message from %s@%s ***\007\n",
	    timeleft ? "": "FINAL ", whom, hostname);

	if (timeleft > 10*60)
		(void)fprintf(pf, "System going down at %5.5s\n\n",
		    ctime(&shuttime) + 11);
	else if (timeleft > 59)
		(void)fprintf(pf, "System going down in %ld minute%s\n\n",
		    (long)timeleft / 60, (timeleft > 60) ? "s" : "");
	else if (timeleft)
		(void)fprintf(pf, "System going down in 30 seconds\n\n");
	else
		(void)fprintf(pf, "System going down IMMEDIATELY\n\n");

	if (mbuflen)
		(void)fwrite(mbuf, 1, mbuflen, pf);

	/*
	 * play some games, just in case wall doesn't come back
	 * probably unnecessary, given that wall is careful.
	 */
	if (!setjmp(alarmbuf)) {
		(void)signal(SIGALRM, timeout);
		(void)alarm((u_int)30);
		(void)pclose(pf);
		(void)alarm((u_int)0);
		(void)signal(SIGALRM, SIG_DFL);
	}
}

static void
/*ARGSUSED*/
timeout(int signo)
{
	longjmp(alarmbuf, 1);
}

static void
die_you_gravy_sucking_pig_dog(void)
{
	const char *what;

	if (doreboot) {
		what = "reboot";
	} else if (dohalt && dopowerdown) {
		what = "poweroff";
	} else if (dohalt) {
		what = "halt";
	} else {
		what = "shutdown";
	}

	syslog(LOG_NOTICE, "%s by %s: %s", what, whom, mbuf);
	(void)sleep(2);

	(void)printf("\r\nSystem shutdown time has arrived\007\007\r\n");
	if (killflg) {
		(void)printf("\rbut you'll have to do it yourself\r\n");
		finish(0);
	}
	if (dofast)
		doitfast();
	dorcshutdown();
	if (doreboot || dohalt) {
		const char *args[20];
		const char **arg, *path;
#ifndef DEBUG
		int serrno;
#endif

		arg = &args[0];
		if (doreboot) {
			path = _PATH_REBOOT;
			*arg++ = "reboot";
		} else {
			path = _PATH_HALT;
			*arg++ = "halt";
		}
		if (doverbose)
			*arg++ = "-v";
		if (dodebug)
			*arg++ = "-x";
		if (dosilent)
			*arg++ = "-z";
		if (dodump)
			*arg++ = "-d";
		if (nosync)
			*arg++ = "-n";
		if (dopowerdown)
			*arg++ = "-p";
		*arg++ = "-l";
		if (bootstr)
			*arg++ = bootstr;
		*arg++ = 0;
#ifndef DEBUG
		(void)unlink(_PATH_NOLOGIN);
		(void)execve(path, __UNCONST(args), NULL);
		serrno = errno;
		syslog(LOG_ERR, "Can't exec `%s' (%m)", path);
		errno = serrno;
		warn("Can't exec `%s'", path);
#else
		printf("%s", path);
		for (arg = &args[0]; *arg; arg++)
			printf(" %s", *arg);
		printf("\n");
#endif
	} else {
#ifndef DEBUG
		(void)kill(1, SIGTERM);		/* to single user */
#else
		printf("kill 1\n");
#endif
	}
	finish(0);
}

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

static void
getoffset(char *timearg)
{
	struct tm *lt;
	char *p;
	time_t now;
	int yearset;

	(void)time(&now);
	if (!strcasecmp(timearg, "now")) {		/* now */
		offset = 0;
		shuttime = now;
		return;
	}

	if (*timearg == '+') {				/* +minutes */
		if (!isdigit((unsigned char)*++timearg))
			badtime();
		offset = atoi(timearg) * 60;
		shuttime = now + offset;
		return;
	}

	/* handle hh:mm by getting rid of the colon */
	for (p = timearg; *p; ++p)
		if (!isascii(*p) || !isdigit((unsigned char)*p)) {
			if (*p == ':' && strlen(p) == 3) {
				p[0] = p[1];
				p[1] = p[2];
				p[2] = '\0';
			}
			else
				badtime();
		}

	(void)unsetenv("TZ");				/* OUR timezone */
	lt = localtime(&now);				/* current time val */

	lt->tm_sec = 0;

	yearset = 0;
	switch (strlen(timearg)) {
	case 12:
		lt->tm_year = ATOI2(timearg) * 100 - TM_YEAR_BASE;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:
		if (yearset) {
			lt->tm_year += ATOI2(timearg);
		} else {
			yearset = ATOI2(timearg);
			if (yearset < 69)
				lt->tm_year = yearset + 2000 - TM_YEAR_BASE;
			else
				lt->tm_year = yearset + 1900 - TM_YEAR_BASE;
		}
		/* FALLTHROUGH */
	case 8:
		lt->tm_mon = ATOI2(timearg);
		--lt->tm_mon;
		/* FALLTHROUGH */
	case 6:
		lt->tm_mday = ATOI2(timearg);
		/* FALLTHROUGH */
	case 4:
		lt->tm_hour = ATOI2(timearg);
		/* FALLTHROUGH */
	case 2:
		lt->tm_min = ATOI2(timearg);
		break;
	default:
		badtime();
	}

	if ((shuttime = mktime(lt)) == -1)
		badtime();
	if ((offset = shuttime - now) < 0)
		errx(1, "time is already past");
}

static void
dorcshutdown(void)
{
	(void)printf("\r\nAbout to run shutdown hooks...\r\n");
#ifndef DEBUG
	(void)setuid(0);
	(void)system(". " _PATH_RCSHUTDOWN);
#endif
	(void)sleep(5);		/* Give operator a chance to abort this. */
	(void)printf("\r\nDone running shutdown hooks.\r\n");
}

#define	FSMSG	"fastboot file for fsck\n"
static void
doitfast(void)
{
	int fastfd;

	if ((fastfd = open(_PATH_FASTBOOT, O_WRONLY|O_CREAT|O_TRUNC,
	    0664)) >= 0) {
		(void)write(fastfd, FSMSG, sizeof(FSMSG) - 1);
		(void)close(fastfd);
	}
}

#define	NOMSG	"\n\nNO LOGINS: System going down at "
static void
nolog(void)
{
	int logfd;
	char *ct;

	(void)unlink(_PATH_NOLOGIN);	/* in case linked to another file */
	(void)signal(SIGINT, finish);
	(void)signal(SIGHUP, finish);
	(void)signal(SIGQUIT, finish);
	(void)signal(SIGTERM, finish);
	if ((logfd = open(_PATH_NOLOGIN, O_WRONLY|O_CREAT|O_TRUNC,
	    0664)) >= 0) {
		(void)write(logfd, NOMSG, sizeof(NOMSG) - 1);
		ct = ctime(&shuttime);
		(void)write(logfd, ct + 11, 5);
		(void)write(logfd, "\n\n", 2);
		(void)write(logfd, mbuf, strlen(mbuf));
		(void)close(logfd);
	}
}

static void
/*ARGSUSED*/
finish(int signo)
{

	if (!killflg)
		(void)unlink(_PATH_NOLOGIN);
	exit(0);
}

static void
badtime(void)
{

	warnx("illegal time format");
	usage();
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "Usage: %s [-Ddfhknprvxz] [-b bootstr] time [message ... | -]\n",
	    getprogname());
	exit(1);
}
