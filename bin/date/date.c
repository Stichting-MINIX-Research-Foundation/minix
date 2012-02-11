/* $NetBSD: date.c,v 1.60 2011/08/27 12:55:09 joerg Exp $ */

/*
 * Copyright (c) 1985, 1987, 1988, 1993
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
__COPYRIGHT(
"@(#) Copyright (c) 1985, 1987, 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)date.c	8.2 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: date.c,v 1.60 2011/08/27 12:55:09 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>

#include "extern.h"

static time_t tval;
static int aflag, jflag, rflag, nflag;

__dead static void badformat(void);
__dead static void badtime(void);
__dead static void badvalue(const char *);
static void setthetime(const char *);
__dead static void usage(void);

int
main(int argc, char *argv[])
{
	char *buf;
	size_t bufsiz;
	const char *format;
	int ch;
	long long val;
	struct tm *tm;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "ad:jnr:u")) != -1) {
		switch (ch) {
		case 'a':		/* adjust time slowly */
			aflag = 1;
			nflag = 1;
			break;
		case 'd':
#ifndef __minix
			rflag = 1;
			tval = parsedate(optarg, NULL, NULL);
			if (tval == -1) 
#endif
badarg:				 errx(EXIT_FAILURE,
				    "Cannot parse `%s'", optarg);
			break;
		case 'j':		/* don't set time */
			jflag = 1;
			break;
		case 'n':		/* don't set network */
			nflag = 1;
			break;
		case 'r':		/* user specified seconds */
			errno = 0;
			val = strtoll(optarg, &buf, 0);
			if (optarg[0] == '\0' || *buf != '\0')
				goto badarg;
			if (errno == ERANGE && (val == LLONG_MAX ||
			    val == LLONG_MIN))
				err(EXIT_FAILURE, "Bad number `%s'", optarg);
			rflag = 1;
			tval = (time_t)val;
			break;
		case 'u':		/* do everything in UTC */
			(void)setenv("TZ", "UTC0", 1);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!rflag && time(&tval) == -1)
		err(EXIT_FAILURE, "time");


	/* allow the operands in any order */
	if (*argv && **argv == '+') {
		format = *argv;
		++argv;
	} else
		format = "+%a %b %e %H:%M:%S %Z %Y";

	if (*argv) {
		setthetime(*argv);
		++argv;
	}

	if (*argv && **argv == '+')
		format = *argv;

	if ((buf = malloc(bufsiz = 1024)) == NULL)
		goto bad;

	if ((tm = localtime(&tval)) == NULL)
		err(EXIT_FAILURE, "localtime %lld failed", (long long)tval);

	while (strftime(buf, bufsiz, format, tm) == 0)
		if ((buf = realloc(buf, bufsiz <<= 1)) == NULL)
			goto bad;

	(void)printf("%s\n", buf + 1);
	free(buf);
	return 0;
bad:
	err(EXIT_FAILURE, "Cannot allocate format buffer");
}

static void
badformat(void)
{
	warnx("illegal time format");
	usage();
}

static void
badtime(void)
{
	errx(EXIT_FAILURE, "illegal time");
	/* NOTREACHED */
}

static void
badvalue(const char *param)
{
	warnx("invalid %s supplied", param);
	usage();
}

#define ATOI2(s) ((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

static void
setthetime(const char *p)
{
	struct timeval tv;
	time_t new_time;
	struct tm *lt;
	const char *dot, *t;
	size_t len;
	int yearset;

	for (t = p, dot = NULL; *t; ++t) {
		if (isdigit((unsigned char)*t))
			continue;
		if (*t == '.' && dot == NULL) {
			dot = t;
			continue;
		}
		badformat();
	}

	if ((lt = localtime(&tval)) == NULL)
		err(EXIT_FAILURE, "localtime %lld failed", (long long)tval);

	lt->tm_isdst = -1;			/* Divine correct DST */

	if (dot != NULL) {			/* .ss */
		len = strlen(dot);
		if (len != 3)
			badformat();
		++dot;
		lt->tm_sec = ATOI2(dot);
		if (lt->tm_sec > 61)
			badvalue("seconds");
	} else {
		len = 0;
		lt->tm_sec = 0;
	}

	yearset = 0;
	switch (strlen(p) - len) {
	case 12:				/* cc */
		lt->tm_year = ATOI2(p) * 100 - TM_YEAR_BASE;
		if (lt->tm_year < 0)
			badtime();
		yearset = 1;
		/* FALLTHROUGH */
	case 10:				/* yy */
		if (yearset) {
			lt->tm_year += ATOI2(p);
		} else {
			yearset = ATOI2(p);
			if (yearset < 69)
				lt->tm_year = yearset + 2000 - TM_YEAR_BASE;
			else
				lt->tm_year = yearset + 1900 - TM_YEAR_BASE;
		}
		/* FALLTHROUGH */
	case 8:					/* mm */
		lt->tm_mon = ATOI2(p);
		if (lt->tm_mon > 12 || lt->tm_mon == 0)
			badvalue("month");
		--lt->tm_mon;			/* time struct is 0 - 11 */
		/* FALLTHROUGH */
	case 6:					/* dd */
		lt->tm_mday = ATOI2(p);
		switch (lt->tm_mon) {
		case 0:
		case 2:
		case 4:
		case 6:
		case 7:
		case 9:
		case 11:
			if (lt->tm_mday > 31 || lt->tm_mday == 0)
				badvalue("day of month");
			break;
		case 3:
		case 5:
		case 8:
		case 10:
			if (lt->tm_mday > 30 || lt->tm_mday == 0)
				badvalue("day of month");
			break;
		case 1:
			if (lt->tm_mday > 29 || lt->tm_mday == 0 ||
			    (lt->tm_mday == 29 &&
			     !isleap(lt->tm_year + TM_YEAR_BASE)))
				badvalue("day of month");
			break;
		default:
			badvalue("month");
			break;
		}
		/* FALLTHROUGH */
	case 4:					/* hh */
		lt->tm_hour = ATOI2(p);
		if (lt->tm_hour > 23)
			badvalue("hour");
		/* FALLTHROUGH */
	case 2:					/* mm */
		lt->tm_min = ATOI2(p);
		if (lt->tm_min > 59)
			badvalue("minute");
		break;
	case 0:					/* was just .sss */
		if (len != 0)
			break;
		/* FALLTHROUGH */
	default:
		badformat();
	}

	/* convert broken-down time to UTC clock time */
	if ((new_time = mktime(lt)) == -1)
		badtime();

	/* if jflag is set, don't actually change the time, just return */
	if (jflag) {
		tval = new_time;
		return;
	}

	/* set the time */
	if (nflag || netsettime(new_time)) {
		logwtmp("|", "date", "");
#if HAVE_ADJTIME
		if (aflag) {
			tv.tv_sec = new_time - tval;
			tv.tv_usec = 0;
			if (adjtime(&tv, NULL))
				err(EXIT_FAILURE, "adjtime");
		}
#else
		if (aflag) {
			err(EXIT_FAILURE, "no adjtime");
		}
#endif
		else {
			tval = new_time;
			tv.tv_sec = tval;
			tv.tv_usec = 0;
			if (settimeofday(&tv, NULL))
				err(EXIT_FAILURE, "settimeofday");
		}
		logwtmp("{", "date", "");
	}

	if ((p = getlogin()) == NULL)
		p = "???";
	syslog(LOG_AUTH | LOG_NOTICE, "date set by %s", p);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-ajnu] [-d date] [-r seconds] [+format]",
	    getprogname());
	(void)fprintf(stderr, " [[[[[[CC]yy]mm]dd]HH]MM[.SS]]\n");
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
