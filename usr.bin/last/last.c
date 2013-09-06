/*	$NetBSD: last.c,v 1.36 2012/03/15 03:04:05 dholland Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)last.c	8.2 (Berkeley) 4/2/94";
#endif
__RCSID("$NetBSD: last.c,v 1.36 2012/03/15 03:04:05 dholland Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <arpa/inet.h>
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif
#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#include <util.h>

#ifndef UT_NAMESIZE
#define UT_NAMESIZE 8
#define UT_LINESIZE 8
#define UT_HOSTSIZE 16
#endif
#ifndef SIGNATURE
#define SIGNATURE -1
#endif



#define	NO	0			/* false/no */
#define	YES	1			/* true/yes */

#define	TBUFLEN	30			/* length of time string buffer */
#define	TFMT	"%a %b %d %R"		/* strftime format string */
#define	LTFMT	"%a %b %d %Y %T"	/* strftime long format string */
#define	TFMTS	"%R"			/* strftime format string - time only */
#define	LTFMTS	"%T"			/* strftime long format string - " */

/* fmttime() flags */
#define	FULLTIME	0x1		/* show year, seconds */
#define	TIMEONLY	0x2		/* show time only, not date */
#define	GMT		0x4		/* show time at GMT, for offsets only */

#define MAXUTMP		1024

typedef struct arg {
	const char	*name;		/* argument */
#define	HOST_TYPE	-2
#define	TTY_TYPE	-3
#define	USER_TYPE	-4
	int		type;		/* type of arg */
	struct arg	*next;		/* linked list pointer */
} ARG;
static ARG	*arglist;		/* head of linked list */

typedef struct ttytab {
	time_t	logout;			/* log out time */
	char	tty[128];		/* terminal name */
	struct ttytab	*next;		/* linked list pointer */
} TTY;
static TTY	*ttylist;		/* head of linked list */

static time_t	currentout;		/* current logout value */
static long	maxrec;			/* records to display */
static int	fulltime = 0;		/* Display seconds? */
static int	xflag;			/* Assume file is wtmpx format */

static void	 addarg(int, const char *);
static TTY	*addtty(const char *);
static void	 hostconv(char *);
static const char *ttyconv(char *);
#ifdef SUPPORT_UTMPX
static void	 wtmpx(const char *, int, int, int, int);
#endif
#ifdef SUPPORT_UTMP
static void	 wtmp(const char *, int, int, int, int);
#endif
static char	*fmttime(time_t, int);
__dead static void	 usage(void);

static
void usage(void)
{
	(void)fprintf(stderr, "usage: %s [-#%s] [-nTx] [-f file]"
	    " [-H hostsize] [-h host] [-L linesize]\n"
	    "\t    [-N namesize] [-t tty] [user ...]\n", getprogname(),
#ifdef NOTYET_SUPPORT_UTMPX
	    "w"
#else
	    ""
#endif
	);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int ch;
	char *p;
	const char *file = NULL;
	int namesize = UT_NAMESIZE;
	int linesize = UT_LINESIZE;
	int hostsize = UT_HOSTSIZE;
	int numeric = 0;

	maxrec = -1;

	while ((ch = getopt(argc, argv, "0123456789f:H:h:L:nN:Tt:x")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: last was originally designed to take
			 * a number after a dash.
			 */
			if (maxrec == -1) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					maxrec = atol(++p);
				else if (optind < argc)
					maxrec = atol(argv[optind] + 1);
				else
					usage();
				if (!maxrec)
					return 0;
			}
			break;
		case 'f':
			file = optarg;
			if ('\0' == file[0])
				usage();
			break;
		case 'H':
			hostsize = atoi(optarg);
			if (hostsize < 1)
				usage();
			break;
		case 'h':
			hostconv(optarg);
			addarg(HOST_TYPE, optarg);
			break;
		case 'L':
			linesize = atoi(optarg);
			if (linesize < 1)
				usage();
			break;
		case 'N':
			namesize = atoi(optarg);
			if (namesize < 1)
				usage();
			break;
		case 'n':
			numeric = 1;
			break;
		case 'T':
			fulltime = 1;
			break;
		case 't':
			addarg(TTY_TYPE, ttyconv(optarg));
			break;
		case 'x':
			xflag = 1;
			break;
		case '?':
		default:
			usage();
		}

	if (argc) {
		setlinebuf(stdout);
		for (argv += optind; *argv; ++argv) {
#define	COMPATIBILITY
#ifdef	COMPATIBILITY
			/* code to allow "last p5" to work */
			addarg(TTY_TYPE, ttyconv(*argv));
#endif
			addarg(USER_TYPE, *argv);
		}
	}
	if (file == NULL) {
#ifdef SUPPORT_UTMPX
		if (access(_PATH_WTMPX, R_OK) == 0)
			file = _PATH_WTMPX;
		else
#endif
#ifdef SUPPORT_UTMP
		if (access(_PATH_WTMP, R_OK) == 0)
			file = _PATH_WTMP;
#endif
		if (file == NULL)
#if defined(SUPPORT_UTMPX) && defined(SUPPORT_UTMP)
			errx(EXIT_FAILURE, "Cannot access `%s' or `%s'", _PATH_WTMPX,
			    _PATH_WTMP);
#elif defined(SUPPORT_UTMPX)
			errx(EXIT_FAILURE, "Cannot access `%s'", _PATH_WTMPX);
#elif defined(SUPPORT_UTMP)
			errx(EXIT_FAILURE, "Cannot access `%s'", _PATH_WTMP);
#else
			errx(EXIT_FAILURE, "No utmp or utmpx support compiled in.");
#endif
	}
#if defined(SUPPORT_UTMPX) && defined(SUPPORT_UTMP)
	if (file[strlen(file) - 1] == 'x' || xflag)
		wtmpx(file, namesize, linesize, hostsize, numeric);
	else
		wtmp(file, namesize, linesize, hostsize, numeric);
#elif defined(SUPPORT_UTMPX)
	wtmpx(file, namesize, linesize, hostsize, numeric);
#elif defined(SUPPORT_UTMP)
	wtmp(file, namesize, linesize, hostsize, numeric);
#else
	errx(EXIT_FAILURE, "No utmp or utmpx support compiled in.");
#endif
	exit(EXIT_SUCCESS);
}


/*
 * addarg --
 *	add an entry to a linked list of arguments
 */
static void
addarg(int type, const char *arg)
{
	ARG *cur;

	if (!(cur = (ARG *)malloc(sizeof(ARG))))
		err(EXIT_FAILURE, "malloc failure");
	cur->next = arglist;
	cur->type = type;
	cur->name = arg;
	arglist = cur;
}

/*
 * addtty --
 *	add an entry to a linked list of ttys
 */
static TTY *
addtty(const char *tty)
{
	TTY *cur;

	if (!(cur = (TTY *)malloc(sizeof(TTY))))
		err(EXIT_FAILURE, "malloc failure");
	cur->next = ttylist;
	cur->logout = currentout;
	memmove(cur->tty, tty, sizeof(cur->tty));
	return (ttylist = cur);
}

/*
 * hostconv --
 *	convert the hostname to search pattern; if the supplied host name
 *	has a domain attached that is the same as the current domain, rip
 *	off the domain suffix since that's what login(1) does.
 */
static void
hostconv(char *arg)
{
	static int first = 1;
	static char *hostdot, name[MAXHOSTNAMELEN + 1];
	char *argdot;

	if (!(argdot = strchr(arg, '.')))
		return;
	if (first) {
		first = 0;
		if (gethostname(name, sizeof(name)))
			err(EXIT_FAILURE, "gethostname");
		name[sizeof(name) - 1] = '\0';
		hostdot = strchr(name, '.');
	}
	if (hostdot && !strcasecmp(hostdot, argdot))
		*argdot = '\0';
}

/*
 * ttyconv --
 *	convert tty to correct name.
 */
static const char *
ttyconv(char *arg)
{
	char *mval;

	if (!strcmp(arg, "co"))
		return ("console");
	/*
	 * kludge -- we assume that all tty's end with
	 * a two character suffix.
	 */
	if (strlen(arg) == 2) {
		if (asprintf(&mval, "tty%s", arg) == -1)
			err(EXIT_FAILURE, "malloc failure");
		return (mval);
	}
	if (!strncmp(arg, _PATH_DEV, sizeof(_PATH_DEV) - 1))
		return (&arg[sizeof(_PATH_DEV) - 1]);
	return (arg);
}

/*
 * fmttime --
 *	return pointer to (static) formatted time string.
 */
static char *
fmttime(time_t t, int flags)
{
	struct tm *tm;
	static char tbuf[TBUFLEN];

	tm = (flags & GMT) ? gmtime(&t) : localtime(&t);
	if (tm == NULL) {
		strcpy(tbuf, "????");
		return tbuf;
	}
	strftime(tbuf, sizeof(tbuf),
	    (flags & TIMEONLY)
	     ? (flags & FULLTIME ? LTFMTS : TFMTS)
	     : (flags & FULLTIME ? LTFMT : TFMT),
	    tm);
	return (tbuf);
}

#ifdef SUPPORT_UTMP
#define TYPE(a)	0
#define NAMESIZE UT_NAMESIZE
#define LINESIZE UT_LINESIZE
#define HOSTSIZE UT_HOSTSIZE
#define ut_timefld ut_time
#define HAS_UT_SS 0
#include "want.c"
#undef TYPE /*(a)*/
#undef NAMESIZE
#undef LINESIZE
#undef HOSTSIZE
#undef ut_timefld
#undef HAS_UT_SS
#endif

#ifdef SUPPORT_UTMPX
#define utmp utmpx
#define want wantx
#define wtmp wtmpx
#define gethost gethostx
#define buf bufx
#define onintr onintrx
#define TYPE(a) (a)->ut_type
#define NAMESIZE UTX_USERSIZE
#define LINESIZE UTX_LINESIZE
#define HOSTSIZE UTX_HOSTSIZE
#define ut_timefld ut_xtime
#define HAS_UT_SS 1
#include "want.c"
#endif
