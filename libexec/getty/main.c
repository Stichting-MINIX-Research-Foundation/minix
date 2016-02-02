/*	$NetBSD: main.c,v 1.64 2013/08/12 13:54:33 joerg Exp $	*/

/*-
 * Copyright (c) 1980, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)main.c	8.1 (Berkeley) 6/20/93";
#else
__RCSID("$NetBSD: main.c,v 1.64 2013/08/12 13:54:33 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/utsname.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <term.h>
#include <termios.h>
#include <time.h>
#include <ttyent.h>
#include <unistd.h>
#include <util.h>

#include "gettytab.h"
#include "pathnames.h"
#include "extern.h"

extern char editedhost[];

/*
 * Set the amount of running time that getty should accumulate
 * before deciding that something is wrong and exit.
 */
#define GETTY_TIMEOUT	60 /* seconds */

/* defines for auto detection of incoming PPP calls (->PAP/CHAP) */

#define PPP_FRAME           0x7e  /* PPP Framing character */
#define PPP_STATION         0xff  /* "All Station" character */
#define PPP_ESCAPE          0x7d  /* Escape Character */
#define PPP_CONTROL         0x03  /* PPP Control Field */
#define PPP_CONTROL_ESCAPED 0x23  /* PPP Control Field, escaped */
#define PPP_LCP_HI          0xc0  /* LCP protocol - high byte */
#define PPP_LCP_LOW         0x21  /* LCP protocol - low byte */

struct termios tmode, omode;

int crmod, digit_or_punc, lower, upper;

char	hostname[MAXHOSTNAMELEN + 1];
struct	utsname kerninfo;
char	name[LOGIN_NAME_MAX];
char	dev[] = _PATH_DEV;
char	ttyn[32];
char	lockfile[512];
uid_t	ttyowner;
char	*rawttyn;

#define	OBUFSIZ		128
#define	TABBUFSIZ	512

char	defent[TABBUFSIZ];
char	tabent[TABBUFSIZ];

char	*env[128];

const unsigned char partab[] = {
	0001,0201,0201,0001,0201,0001,0001,0201,
	0202,0004,0003,0205,0005,0206,0201,0001,
	0201,0001,0001,0201,0001,0201,0201,0001,
	0001,0201,0201,0001,0201,0001,0001,0201,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0201
};

#define	ERASE	tmode.c_cc[VERASE]
#define	KILL	tmode.c_cc[VKILL]
#define	EOT	tmode.c_cc[VEOF]

static void	clearscreen(void);

sigjmp_buf timeout;

__dead static void
/*ARGSUSED*/
dingdong(int signo)
{

	(void)alarm(0);
	(void)signal(SIGALRM, SIG_DFL);
	siglongjmp(timeout, 1);
}

sigjmp_buf intrupt;

__dead static void
/*ARGSUSED*/
interrupt(int signo)
{

	(void)signal(SIGINT, interrupt);
	siglongjmp(intrupt, 1);
}

#if !defined(__minix)
/*
 * Action to take when getty is running too long.
 */
__dead static void
/*ARGSUSED*/
timeoverrun(int signo)
{

	syslog(LOG_ERR, "getty exiting due to excessive running time");
	exit(1);
}
#endif /* !defined(__minix) */

static int	getname(void);
static void	oflush(void);
static void	prompt(void);
static int	putchr(int);
static void	putf(const char *);
static void	xputs(const char *);

#define putpad(s) tputs(s, 1, putchr)

int
main(int argc, char *argv[], char *envp[])
{
	const char *progname;
	int repcnt = 0, failopenlogged = 0, first_time = 1;
	struct rlimit limit;
	struct passwd *pw;
	int rval;
	/* this is used past the siglongjmp, so make sure it is not cached
	   in registers that might become invalid. */
	volatile int uugetty = 0;
	const char * volatile tname = "default";

	(void)signal(SIGINT, SIG_IGN);
	openlog("getty", LOG_PID, LOG_AUTH);
	(void)gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';
	if (hostname[0] == '\0')
		(void)strlcpy(hostname, "Amnesiac", sizeof(hostname));
	(void)uname(&kerninfo);

	progname = getprogname();
	if (progname[0] == 'u' && progname[1] == 'u')
		uugetty = 1;

	/*
	 * Find id of uucp login (if present) so we can chown tty properly.
	 */
	if (uugetty && (pw = getpwnam("uucp")))
		ttyowner = pw->pw_uid;
	else
		ttyowner = 0;

	/*
	 * Limit running time to deal with broken or dead lines.
	 */
#if !defined(__minix)
	(void)signal(SIGXCPU, timeoverrun);
#endif /* !defined(__minix) */
	limit.rlim_max = RLIM_INFINITY;
	limit.rlim_cur = GETTY_TIMEOUT;
	(void)setrlimit(RLIMIT_CPU, &limit);

	/*
	 * The following is a work around for vhangup interactions
	 * which cause great problems getting window systems started.
	 * If the tty line is "-", we do the old style getty presuming
	 * that the file descriptors are already set up for us.
	 * J. Gettys - MIT Project Athena.
	 */
	if (argc <= 2 || strcmp(argv[2], "-") == 0) {
		(void)strlcpy(ttyn, ttyname(0), sizeof(ttyn));
	}
	else {
		int i;

		rawttyn = argv[2];
		(void)strlcpy(ttyn, dev, sizeof(ttyn));
		(void)strlcat(ttyn, argv[2], sizeof(ttyn));
		if (uugetty)  {
			(void)chown(ttyn, ttyowner, 0);
			(void)strlcpy(lockfile, _PATH_LOCK,
				sizeof(lockfile));
			(void)strlcat(lockfile, argv[2],
				sizeof(lockfile));
			/*
			 * wait for lockfiles to go away before we try
			 * to open
			 */
			if (pidlock(lockfile, 0, 0, 0) != 0)  {
				syslog(LOG_ERR,
					"%s: can't create lockfile", ttyn);
				exit(1);
			}
			(void)unlink(lockfile);
		}
		if (strcmp(argv[0], "+") != 0) {
			(void)chown(ttyn, ttyowner, 0);
			(void)chmod(ttyn, 0600);
#if !defined(__minix)
			(void)revoke(ttyn);
#endif /* !defined(__minix) */
			if (ttyaction(ttyn, "getty", "root"))
				syslog(LOG_WARNING, "%s: ttyaction failed",
					ttyn);
			/*
			 * Delay the open so DTR stays down long enough
			 * to be detected.
			 */
			(void)sleep(2);
			while ((i = open(ttyn, O_RDWR)) == -1) {
				if ((repcnt % 10 == 0) &&
				    (errno != ENXIO || !failopenlogged)) {
					syslog(LOG_WARNING, "%s: %m", ttyn);
					closelog();
					failopenlogged = 1;
				}
				repcnt++;
				(void)sleep(60);
			}
			if (uugetty && pidlock(lockfile, 0, 0, 0) != 0)  {
				syslog(LOG_ERR, "%s: can't create lockfile",
					ttyn);
				exit(1);
			}
			if (uugetty)
				(void)chown(lockfile, ttyowner, 0);
			(void)login_tty(i);
		}
	}

	/* Start with default tty settings */
	if (tcgetattr(0, &tmode) < 0) {
		syslog(LOG_ERR, "%s: %m", ttyn);
		exit(1);
	}
	omode = tmode;

	gettable("default", defent);
	gendefaults();
	if (argc > 1)
		tname = argv[1];
	for (;;) {
#if !defined(__minix)
		int off;
#endif /* !defined(__minix) */

		rval = 0;
		gettable(tname, tabent);
		if (OPset || EPset || APset)
			APset++, OPset++, EPset++;
		setdefaults();
#if !defined(__minix)
		off = 0;
#endif /* !defined(__minix) */
		(void)tcflush(0, TCIOFLUSH);	/* clear out the crap */
#if !defined(__minix)
		(void)ioctl(0, FIONBIO, &off);	/* turn off non-blocking mode */
		(void)ioctl(0, FIOASYNC, &off);	/* ditto for async mode */
#endif /* !defined(__minix) */

		if (IS)
			(void)cfsetispeed(&tmode, (speed_t)IS);
		else if (SP)
			(void)cfsetispeed(&tmode, (speed_t)SP);
		if (OS)
			(void)cfsetospeed(&tmode, (speed_t)OS);
		else if (SP)
			(void)cfsetospeed(&tmode, (speed_t)SP);
		setflags(0);
		setchars();
		if (tcsetattr(0, TCSANOW, &tmode) < 0) {
			syslog(LOG_ERR, "%s: %m", ttyn);
			exit(1);
		}
		if (AB) {
			tname = autobaud();
			continue;
		}
		if (PS) {
			tname = portselector();
			continue;
		}
		if (CS)
			clearscreen();
		if (CL && *CL)
			putpad(CL);
		edithost(HE);

		/*
		 * If this is the first time through this, and an
		 * issue file has been given, then send it.
		 */
		if (first_time != 0 && IF != NULL) {
			char buf[_POSIX2_LINE_MAX];
			FILE *fp;

			if ((fp = fopen(IF, "r")) != NULL) {
				while (fgets(buf, sizeof(buf) - 1, fp) != NULL)
					putf(buf);
				(void)fclose(fp);
			}
		}
		first_time = 0;

		if (IM && *IM)
			putf(IM);
		oflush();
		if (sigsetjmp(timeout, 1)) {
			tmode.c_ispeed = tmode.c_ospeed = 0;
			(void)tcsetattr(0, TCSANOW, &tmode);
			exit(1);
		}
		if (TO) {
			(void)signal(SIGALRM, dingdong);
			(void)alarm((unsigned int)TO);
		}
		if (NN) {
			name[0] = '\0';
			lower = 1;
			upper = digit_or_punc = 0;
		} else if (AL) {
			const char *p = AL;
			char *q = name;

			while (*p && q < &name[sizeof name - 1]) {
				if (isupper((unsigned char)*p))
					upper = 1;
				else if (islower((unsigned char)*p))
					lower = 1;
				else if (isdigit((unsigned char)*p))
					digit_or_punc = 1;
				*q++ = *p++;
			}
		} else if ((rval = getname()) == 2) {
			setflags(2);
			(void)execle(PP, "ppplogin", ttyn, (char *) 0, env);
			syslog(LOG_ERR, "%s: %m", PP);
			exit(1);
		}

		if (rval || AL || NN) {
			int i;

			oflush();
			(void)alarm(0);
			(void)signal(SIGALRM, SIG_DFL);
			if (name[0] == '-') {
				xputs("user names may not start with '-'.");
				continue;
			}
			if (!(upper || lower || digit_or_punc))
				continue;
			setflags(2);
			if (crmod) {
				tmode.c_iflag |= ICRNL;
				tmode.c_oflag |= ONLCR;
			}
#if XXX
			if (upper || UC)
				tmode.sg_flags |= LCASE;
			if (lower || LC)
				tmode.sg_flags &= ~LCASE;
#endif
			if (tcsetattr(0, TCSANOW, &tmode) < 0) {
				syslog(LOG_ERR, "%s: %m", ttyn);
				exit(1);
			}
			(void)signal(SIGINT, SIG_DFL);
			for (i = 0; envp[i] != NULL; i++)
				env[i] = envp[i];
			makeenv(&env[i]);

			limit.rlim_max = RLIM_INFINITY;
			limit.rlim_cur = RLIM_INFINITY;
			(void)setrlimit(RLIMIT_CPU, &limit);
			if (NN)
				(void)execle(LO, "login", AL ? "-fp" : "-p",
				    NULL, env);
			else
				(void)execle(LO, "login", AL ? "-fp" : "-p",
				    "--", name, NULL, env);
			syslog(LOG_ERR, "%s: %m", LO);
			exit(1);
		}
		(void)alarm(0);
		(void)signal(SIGALRM, SIG_DFL);
		(void)signal(SIGINT, SIG_IGN);
		if (NX && *NX)
			tname = NX;
		if (uugetty)
			(void)unlink(lockfile);
	}
}

static int
getname(void)
{
	int c;
	char *np;
	unsigned char cs;
	int ppp_state, ppp_connection;

	/*
	 * Interrupt may happen if we use CBREAK mode
	 */
	if (sigsetjmp(intrupt, 1)) {
		(void)signal(SIGINT, SIG_IGN);
		return (0);
	}
	(void)signal(SIGINT, interrupt);
	setflags(1);
	prompt();
	if (PF > 0) {
		oflush();
		(void)sleep((unsigned int)PF);
		PF = 0;
	}
	if (tcsetattr(0, TCSANOW, &tmode) < 0) {
		syslog(LOG_ERR, "%s: %m", ttyn);
		exit(1);
	}
	crmod = digit_or_punc = lower = upper = 0;
	ppp_state = ppp_connection = 0;
	np = name;
	for (;;) {
		oflush();
		if (read(STDIN_FILENO, &cs, 1) <= 0)
			exit(0);
		if ((c = cs&0177) == 0)
			return (0);

		/*
		 * PPP detection state machine..
		 * Look for sequences:
		 * PPP_FRAME, PPP_STATION, PPP_ESCAPE, PPP_CONTROL_ESCAPED or
		 * PPP_FRAME, PPP_STATION, PPP_CONTROL (deviant from RFC)
		 * See RFC1662.
		 * Derived from code from Michael Hancock <michaelh@cet.co.jp>
		 * and Erik 'PPP' Olson <eriko@wrq.com>
		 */
		if (PP && cs == PPP_FRAME) {
			ppp_state = 1;
		} else if (ppp_state == 1 && cs == PPP_STATION) {
			ppp_state = 2;
		} else if (ppp_state == 2 && cs == PPP_ESCAPE) {
			ppp_state = 3;
		} else if ((ppp_state == 2 && cs == PPP_CONTROL) ||
		    (ppp_state == 3 && cs == PPP_CONTROL_ESCAPED)) {
			ppp_state = 4;
		} else if (ppp_state == 4 && cs == PPP_LCP_HI) {
			ppp_state = 5;
		} else if (ppp_state == 5 && cs == PPP_LCP_LOW) {
			ppp_connection = 1;
			break;
		} else {
			ppp_state = 0;
		}

		if (c == EOT)
			exit(1);
		if (c == '\r' || c == '\n' ||
		    np >= &name[LOGIN_NAME_MAX - 1]) {
			*np = '\0';
			putf("\r\n");
			break;
		}
		if (islower(c))
			lower = 1;
		else if (isupper(c))
			upper = 1;
		else if (c == ERASE || c == '#' || c == '\b') {
			if (np > name) {
				np--;
				if (cfgetospeed(&tmode) >= 1200)
					xputs("\b \b");
				else
					putchr(cs);
			}
			continue;
		} else if (c == KILL || c == '@') {
			putchr(cs);
			putchr('\r');
			if (cfgetospeed(&tmode) < 1200)
				putchr('\n');
			/* this is the way they do it down under ... */
			else if (np > name)
				xputs(
				    "                                     \r");
			prompt();
			np = name;
			continue;
		} else if (isdigit(c) || c == '_')
			digit_or_punc = 1;
		if (IG && (c <= ' ' || c > 0176))
			continue;
		*np++ = c;
		putchr(cs);

		/*
		 * An MS-Windows direct connect PPP "client" won't send its
		 * first PPP packet until we respond to its "CLIENT" poll
		 * with a CRLF sequence.  We cater to yet another broken
		 * implementation of a previously-standard protocol...
		 */
		*np = '\0';
		if (strstr(name, "CLIENT"))
			putf("\r\n");
	}
	(void)signal(SIGINT, SIG_IGN);
	*np = 0;
	if (c == '\r')
		crmod = 1;
	if ((upper && !lower && !LC) || UC)
		for (np = name; *np; np++)
			*np = tolower((unsigned char)*np);
	return (1 + ppp_connection);
}

static void
xputs(const char *s)
{
	while (*s)
		putchr(*s++);
}

char	outbuf[OBUFSIZ];
size_t	obufcnt = 0;

static int
putchr(int cc)
{
	unsigned char c;

	c = cc;
	if (!NP) {
		c |= partab[c&0177] & 0200;
		if (OP)
			c ^= 0200;
	}
	if (!UB) {
		outbuf[obufcnt++] = c;
		if (obufcnt >= OBUFSIZ)
			oflush();
		return 1;
	}
	return write(STDOUT_FILENO, &c, 1);
}

static void
oflush(void)
{
	if (obufcnt)
		(void)write(STDOUT_FILENO, outbuf, obufcnt);
	obufcnt = 0;
}

static void
prompt(void)
{

	putf(LM);
	if (CO)
		putchr('\n');
}

static void
putf(const char *cp)
{
	time_t t;
	char *slash, db[100];

	while (*cp) {
		if (*cp != '%') {
			putchr(*cp++);
			continue;
		}
		switch (*++cp) {

		case 't':
			if ((slash = strstr(ttyn, "/pts/")) == NULL)
				slash = strrchr(ttyn, '/');
			if (slash == NULL)
				xputs(ttyn);
			else
				xputs(&slash[1]);
			break;

		case 'h':
			xputs(editedhost);
			break;

		case 'd':
			(void)time(&t);
			(void)strftime(db, sizeof(db),
			    "%l:%M%p on %A, %d %B %Y", localtime(&t));
			xputs(db);
			break;

		case 's':
			xputs(kerninfo.sysname);
			break;

		case 'm':
			xputs(kerninfo.machine);
			break;

		case 'r':
			xputs(kerninfo.release);
			break;

		case 'v':
			xputs(kerninfo.version);
			break;

		case '%':
			putchr('%');
			break;
		}
		if (*cp)
			cp++;
	}
}

static void
clearscreen(void)
{
	struct ttyent *typ;
	int err;

	if (rawttyn == NULL)
		return;

	typ = getttynam(rawttyn);

	if ((typ == NULL) || (typ->ty_type == NULL) ||
	    (typ->ty_type[0] == 0))
		return;

	if (setupterm(typ->ty_type, 0, &err) == ERR)
		return;

	if (clear_screen)
		putpad(clear_screen);

	del_curterm(cur_term);
	cur_term = NULL;
}
