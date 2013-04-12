/*	$OpenBSD: touch.c,v 1.17 2007/08/06 19:16:06 sobrado Exp $	*/
/*	$NetBSD: touch.c,v 1.11 1995/08/31 22:10:06 jtc Exp $	*/

/*
 * Copyright (c) 1993
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <utime.h>
#include <tzfile.h>
#include <unistd.h>

void		stime_arg1(char *, struct timeval *);
void		stime_arg2(char *, int, struct timeval *);
void		stime_file(char *, struct timeval *);
__dead void	usage(void);

char * __progname;

int
main(int argc, char *argv[])
{
	struct stat	 sb;
	struct timeval	 tv[2];
	int		 aflag, cflag, mflag, ch, fd, len, rval, timeset;
	char		*p;

	(void)setlocale(LC_ALL, "");
	__progname = argv[0];

	aflag = cflag = mflag = timeset = 0;
	if (gettimeofday(&tv[0], NULL))
		err(1, "gettimeofday");

	while ((ch = getopt(argc, argv, "acfmr:t:")) != -1)
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'f':
			break;
		case 'm':
			mflag = 1;
			break;
		case 'r':
			timeset = 1;
			stime_file(optarg, tv);
			break;
		case 't':
			timeset = 1;
			stime_arg1(optarg, tv);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Default is both -a and -m. */
	if (aflag == 0 && mflag == 0)
		aflag = mflag = 1;

	/*
	 * If no -r or -t flag, at least two operands, the first of which
	 * is an 8 or 10 digit number, use the obsolete time specification.
	 */
	if (!timeset && argc > 1) {
		(void)strtol(argv[0], &p, 10);
		len = p - argv[0];
		if (*p == '\0' && (len == 8 || len == 10)) {
			timeset = 1;
			stime_arg2(*argv++, len == 10, tv);
		}
	}

	/* Otherwise use the current time of day. */
	if (!timeset)
		tv[1] = tv[0];

	if (*argv == NULL)
		usage();

	for (rval = 0; *argv; ++argv) {
		/* See if the file exists. */
		if (stat(*argv, &sb)) {
			if (!cflag) {
				/* Create the file. */
				fd = open(*argv,
				    O_WRONLY | O_CREAT, DEFFILEMODE);
				if (fd == -1 || fstat(fd, &sb) || close(fd)) {
					rval = 1;
					warn("%s", *argv);
					continue;
				}

				/* If using the current time, we're done. */
				if (!timeset)
					continue;
			} else
				continue;
		}

		if (!aflag)
			TIMESPEC_TO_TIMEVAL(&tv[0], &sb.st_atimespec);
		if (!mflag)
			TIMESPEC_TO_TIMEVAL(&tv[1], &sb.st_mtimespec);

		/* Try utimes(2). */
		if (!utimes(*argv, tv))
			continue;

		/* If the user specified a time, nothing else we can do. */
		if (timeset) {
			rval = 1;
			warn("%s", *argv);
		}

		/*
		 * System V and POSIX 1003.1 require that a NULL argument
		 * set the access/modification times to the current time.
		 * The permission checks are different, too, in that the
		 * ability to write the file is sufficient.  Take a shot.
		 */
		 if (!utimes(*argv, NULL))
			continue;

		rval = 1;
		warn("%s", *argv);
	}
	exit(rval);
}

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

void
stime_arg1(char *arg, struct timeval *tvp)
{
	struct tm	*lt;
	time_t		 tmptime;
	int		 yearset;
	char		*dot, *p;
					/* Start with the current time. */
	tmptime = tvp[0].tv_sec;
	if ((lt = localtime(&tmptime)) == NULL)
		err(1, "localtime");
					/* [[CC]YY]MMDDhhmm[.SS] */
	for (p = arg, dot = NULL; *p != '\0'; p++) {
		if (*p == '.' && dot == NULL)
			dot = p;
		else if (!isdigit((unsigned char)*p))
			goto terr;
	}
	if (dot == NULL)
		lt->tm_sec = 0;		/* Seconds defaults to 0. */
	else {
		*dot++ = '\0';
		if (strlen(dot) != 2)
			goto terr;
		lt->tm_sec = ATOI2(dot);
		if (lt->tm_sec > 61)	/* Could be leap second. */
			goto terr;
	}

	yearset = 0;
	switch (strlen(arg)) {
	case 12:			/* CCYYMMDDhhmm */
		lt->tm_year = ATOI2(arg) * 100 - TM_YEAR_BASE;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:			/* YYMMDDhhmm */
		if (yearset) {
			yearset = ATOI2(arg);
			lt->tm_year += yearset;
		} else {
			yearset = ATOI2(arg);
			/* Preserve current century. */
			lt->tm_year = ((lt->tm_year / 100) * 100) + yearset;
		}
		/* FALLTHROUGH */
	case 8:				/* MMDDhhmm */
		lt->tm_mon = ATOI2(arg);
		if (lt->tm_mon > 12 || lt->tm_mon == 0)
			goto terr;
		--lt->tm_mon;		/* Convert from 01-12 to 00-11 */
		lt->tm_mday = ATOI2(arg);
		if (lt->tm_mday > 31 || lt->tm_mday == 0)
			goto terr;
		lt->tm_hour = ATOI2(arg);
		if (lt->tm_hour > 23)
			goto terr;
		lt->tm_min = ATOI2(arg);
		if (lt->tm_min > 59)
			goto terr;
		break;
	default:
		goto terr;
	}

	lt->tm_isdst = -1;		/* Figure out DST. */
	tvp[0].tv_sec = tvp[1].tv_sec = mktime(lt);
	if (tvp[0].tv_sec == -1)
terr:		errx(1,
	"out of range or illegal time specification: [[CC]YY]MMDDhhmm[.SS]");

	tvp[0].tv_usec = tvp[1].tv_usec = 0;
}

void
stime_arg2(char *arg, int year, struct timeval *tvp)
{
	struct tm	*lt;
	time_t		 tmptime;
					/* Start with the current time. */
	tmptime = tvp[0].tv_sec;
	if ((lt = localtime(&tmptime)) == NULL)
		err(1, "localtime");

	lt->tm_mon = ATOI2(arg);	/* MMDDhhmm[YY] */
	if (lt->tm_mon > 12 || lt->tm_mon == 0)
		goto terr;
	--lt->tm_mon;			/* Convert from 01-12 to 00-11 */
	lt->tm_mday = ATOI2(arg);
	if (lt->tm_mday > 31 || lt->tm_mday == 0)
		goto terr;
	lt->tm_hour = ATOI2(arg);
	if (lt->tm_hour > 23)
		goto terr;
	lt->tm_min = ATOI2(arg);
	if (lt->tm_min > 59)
		goto terr;
	if (year) {
		year = ATOI2(arg);	/* Preserve current century. */
		lt->tm_year = ((lt->tm_year / 100) * 100) + year;
	}
	lt->tm_sec = 0;

	lt->tm_isdst = -1;		/* Figure out DST. */
	tvp[0].tv_sec = tvp[1].tv_sec = mktime(lt);
	if (tvp[0].tv_sec == -1)
terr:		errx(1,
	"out of range or illegal time specification: MMDDhhmm[YY]");

	tvp[0].tv_usec = tvp[1].tv_usec = 0;
}

void
stime_file(char *fname, struct timeval *tvp)
{
	struct stat	sb;

	if (stat(fname, &sb))
		err(1, "%s", fname);
	TIMESPEC_TO_TIMEVAL(tvp, &sb.st_atimespec);
	TIMESPEC_TO_TIMEVAL(tvp + 1, &sb.st_mtimespec);
}

__dead void
usage(void)
{
	extern char	*__progname;

	(void)fprintf(stderr,
	    "usage: %s [-acm] [-r file] [-t [[CC]YY]MMDDhhmm[.SS]] file ...\n",
	    __progname);
	exit(1);
}
