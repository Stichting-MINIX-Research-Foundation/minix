/*	$NetBSD: cal.c,v 1.27 2011/08/29 13:55:22 joerg Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kim Letkeman.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)cal.c	8.4 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: cal.c,v 1.27 2011/08/29 13:55:22 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>

#define	SATURDAY 		6		/* 1 Jan 1 was a Saturday */

#define	FIRST_MISSING_DAY 	reform->first_missing_day
#define	NUMBER_MISSING_DAYS 	reform->missing_days

#define	MAXDAYS			42		/* max slots in a month array */
#define	SPACE			-1		/* used in day array */

static int days_in_month[2][13] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

static int empty[MAXDAYS] = {
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
};
static int shift_days[2][4][MAXDAYS + 1];

static const char *month_names[12] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December",
};

static const char *day_headings = " S  M Tu  W Th  F  S";
static const char *j_day_headings = "  S   M  Tu   W  Th   F   S";

/* leap years according to the julian calendar */
#define j_leap_year(y, m, d) \
	(((m) > 2) && \
	 !((y) % 4))

/* leap years according to the gregorian calendar */
#define g_leap_year(y, m, d) \
	(((m) > 2) && \
	 ((!((y) % 4) && ((y) % 100)) || \
	  !((y) % 400)))

/* leap year -- account for gregorian reformation at some point */
#define	leap_year(yr) \
	((yr) <= reform->year ? j_leap_year((yr), 3, 1) : \
	g_leap_year((yr), 3, 1))

/* number of julian leap days that have passed by a given date */
#define j_leap_days(y, m, d) \
	((((y) - 1) / 4) + j_leap_year(y, m, d))

/* number of gregorian leap days that have passed by a given date */
#define g_leap_days(y, m, d) \
	((((y) - 1) / 4) - (((y) - 1) / 100) + (((y) - 1) / 400) + \
	g_leap_year(y, m, d))

/*
 * Subtracting the gregorian leap day count (for a given date) from
 * the julian leap day count (for the same date) describes the number
 * of days from the date before the shift to the next date that
 * appears in the calendar.  Since we want to know the number of
 * *missing* days, not the number of days that the shift spans, we
 * subtract 2.
 *
 * Alternately...
 *
 * There's a reason they call the Dark ages the Dark Ages.  Part of it
 * is that we don't have that many records of that period of time.
 * One of the reasons for this is that a lot of the Dark Ages never
 * actually took place.  At some point in the first millenium A.D., a
 * ruler of some power decided that he wanted the number of the year
 * to be different than what it was, so he changed it to coincide
 * nicely with some event (a birthday or anniversary, perhaps a
 * wedding, or maybe a centennial for a largish city).  One of the
 * side effects of this upon the Gregorian reform is that two Julian
 * leap years (leap days celebrated during centennial years that are
 * not quatro-centennial years) were skipped.
 */
#define GREGORIAN_MAGIC 2

/* number of centuries since the reform, not inclusive */
#define	centuries_since_reform(yr) \
	((yr) > reform->year ? ((yr) / 100) - (reform->year / 100) : 0)

/* number of centuries since the reform whose modulo of 400 is 0 */
#define	quad_centuries_since_reform(yr) \
	((yr) > reform->year ? ((yr) / 400) - (reform->year / 400) : 0)

/* number of leap years between year 1 and this year, not inclusive */
#define	leap_years_since_year_1(yr) \
	((yr) / 4 - centuries_since_reform(yr) + quad_centuries_since_reform(yr))

static struct reform {
	const char *country;
	int ambiguity, year, month, date;
	long first_missing_day;
	int missing_days;
	/*
	 * That's 2 for standard/julian display, 4 for months possibly
	 * affected by the Gregorian shift, and MAXDAYS + 1 for the
	 * days that get displayed, plus a crib slot.
	 */
} *reform, reforms[] = {
	{ "DEFAULT",		0, 1752,  9,  3, 0, 0 },
	{ "Italy",		1, 1582, 10,  5, 0, 0 },
	{ "Spain",		1, 1582, 10,  5, 0, 0 },
	{ "Portugal",		1, 1582, 10,  5, 0, 0 },
	{ "Poland",		1, 1582, 10,  5, 0, 0 },
	{ "France",		2, 1582, 12, 10, 0, 0 },
	{ "Luxembourg",		2, 1582, 12, 22, 0, 0 },
	{ "Netherlands",	2, 1582, 12, 22, 0, 0 },
	{ "Bavaria",		0, 1583, 10,  6, 0, 0 },
	{ "Austria",		2, 1584,  1,  7, 0, 0 },
	{ "Switzerland",	2, 1584,  1, 12, 0, 0 },
	{ "Hungary",		0, 1587, 10, 22, 0, 0 },
	{ "Germany",		0, 1700,  2, 19, 0, 0 },
	{ "Norway",		0, 1700,  2, 19, 0, 0 },
	{ "Denmark",		0, 1700,  2, 19, 0, 0 },
	{ "Great Britain",	0, 1752,  9,  3, 0, 0 },
	{ "England",		0, 1752,  9,  3, 0, 0 },
	{ "America",		0, 1752,  9,  3, 0, 0 },
	{ "Sweden",		0, 1753,  2, 18, 0, 0 },
	{ "Finland",		0, 1753,  2, 18, 0, 0 },
	{ "Japan",		0, 1872, 12, 20, 0, 0 },
	{ "China",		0, 1911, 11,  7, 0, 0 },
	{ "Bulgaria",		0, 1916,  4,  1, 0, 0 },
	{ "U.S.S.R.",		0, 1918,  2,  1, 0, 0 },
	{ "Serbia",		0, 1919,  1, 19, 0, 0 },
	{ "Romania",		0, 1919,  1, 19, 0, 0 },
	{ "Greece",		0, 1924,  3, 10, 0, 0 },
	{ "Turkey",		0, 1925, 12, 19, 0, 0 },
	{ "Egypt",		0, 1928,  9, 18, 0, 0 },
	{ NULL,			0,    0,  0,  0, 0, 0 },
};

static int julian;
static int dow;
static int hilite;
static const char *md, *me;

static void	init_hilite(void);
static int	getnum(const char *);
static void	gregorian_reform(const char *);
static void	reform_day_array(int, int, int *, int *, int *,int *,int *,int *);
static int	ascii_day(char *, int);
static void	center(const char *, int, int);
static void	day_array(int, int, int *);
static int	day_in_week(int, int, int);
static int	day_in_year(int, int, int);
static void	monthrange(int, int, int, int, int);
static void	trim_trailing_spaces(char *);
__dead static void	usage(void);

int
main(int argc, char **argv)
{
	struct tm *local_time;
	time_t now;
	int ch, yflag;
	long month, year;
	int before, after, use_reform;
	int yearly = 0;
	char *when, *eoi;

	before = after = 0;
	use_reform = yflag = year = 0;
	when = NULL;
	while ((ch = getopt(argc, argv, "A:B:d:hjR:ry3")) != -1) {
		switch (ch) {
		case 'A':
			after = getnum(optarg);
			if (after < 0)
				errx(1, "Argument to -A must be positive");
			break;
		case 'B':
			before = getnum(optarg);
			if (before < 0)
				errx(1, "Argument to -B must be positive");
			break;
		case 'd':
			dow = getnum(optarg);
			if (dow < 0 || dow > 6)
				errx(1, "illegal day of week value: use 0-6");
			break;
		case 'h':
			init_hilite();
			break;
		case 'j':
			julian = 1;
			break;
		case 'R':
			when = optarg;
			break;
		case 'r':
			use_reform = 1;
			break;
		case 'y':
			yflag = 1;
			break;
		case '3':
			before = after = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (when != NULL)
		gregorian_reform(when);
	if (reform == NULL)
		gregorian_reform("DEFAULT");

	month = 0;
	switch (argc) {
	case 2:
		month = strtol(*argv++, &eoi, 10);
		if (month < 1 || month > 12 || *eoi != '\0')
			errx(1, "illegal month value: use 1-12");
		year = strtol(*argv, &eoi, 10);
		if (year < 1 || year > 9999 || *eoi != '\0')
			errx(1, "illegal year value: use 1-9999");
		break;
	case 1:
		year = strtol(*argv, &eoi, 10);
		if (year < 1 || year > 9999 || (*eoi != '\0' && *eoi != '/' && *eoi != '-'))
			errx(1, "illegal year value: use 1-9999");
		if (*eoi != '\0') {
			month = strtol(eoi + 1, &eoi, 10);
			if (month < 1 || month > 12 || *eoi != '\0')
				errx(1, "illegal month value: use 1-12");
		}
		break;
	case 0:
		(void)time(&now);
		local_time = localtime(&now);
		if (use_reform)
			year = reform->year;
		else
			year = local_time->tm_year + TM_YEAR_BASE;
		if (!yflag) {
			if (use_reform)
				month = reform->month;
			else
				month = local_time->tm_mon + 1;
		}
		break;
	default:
		usage();
	}

	if (!month) {
		/* yearly */
		month = 1;
		before = 0;
		after = 11;
		yearly = 1;
	}

	monthrange(month, year, before, after, yearly);

	exit(0);
}

#define	DAY_LEN		3		/* 3 spaces per day */
#define	J_DAY_LEN	4		/* 4 spaces per day */
#define	WEEK_LEN	20		/* 7 * 3 - one space at the end */
#define	J_WEEK_LEN	27		/* 7 * 4 - one space at the end */
#define	HEAD_SEP	2		/* spaces between day headings */
#define	J_HEAD_SEP	2
#define	MONTH_PER_ROW	3		/* how many monthes in a row */
#define	J_MONTH_PER_ROW	2

static void
monthrange(int month, int year, int before, int after, int yearly)
{
	int startmonth, startyear;
	int endmonth, endyear;
	int i, row;
	int days[3][MAXDAYS];
	char lineout[256];
	int inayear;
	int newyear;
	int day_len, week_len, head_sep;
	int month_per_row;
	int skip, r_off, w_off;

	if (julian) {
		day_len = J_DAY_LEN;
		week_len = J_WEEK_LEN;
		head_sep = J_HEAD_SEP;
		month_per_row = J_MONTH_PER_ROW;
	}
	else {
		day_len = DAY_LEN;
		week_len = WEEK_LEN;
		head_sep = HEAD_SEP;
		month_per_row = MONTH_PER_ROW;
	}

	month--;

	startyear = year - (before + 12 - 1 - month) / 12;
	startmonth = 12 - 1 - ((before + 12 - 1 - month) % 12);
	endyear = year + (month + after) / 12;
	endmonth = (month + after) % 12;

	if (startyear < 0 || endyear > 9999) {
		errx(1, "year should be in 1-9999\n");
	}

	year = startyear;
	month = startmonth;
	inayear = newyear = (year != endyear || yearly);
	if (inayear) {
		skip = month % month_per_row;
		month -= skip;
	}
	else {
		skip = 0;
	}

	do {
		if (newyear) {
			(void)snprintf(lineout, sizeof(lineout), "%d", year);
			center(lineout, week_len * month_per_row +
			    head_sep * (month_per_row - 1), 0);
			(void)printf("\n\n");
			newyear = 0;
		}

		for (i = 0; i < skip; i++)
			center("", week_len, head_sep);

		for (; i < month_per_row; i++) {
			int sep;

			if (year == endyear && month + i > endmonth)
				break;

			sep = (i == month_per_row - 1) ? 0 : head_sep;
			day_array(month + i + 1, year, days[i]);
			if (inayear) {
				center(month_names[month + i], week_len, sep);
			}
			else {
				snprintf(lineout, sizeof(lineout), "%s %d",
				    month_names[month + i], year);
				center(lineout, week_len, sep);
			}
		}
		printf("\n");

		for (i = 0; i < skip; i++)
			center("", week_len, head_sep);

		for (; i < month_per_row; i++) {
			int sep;

			if (year == endyear && month + i > endmonth)
				break;

			sep = (i == month_per_row - 1) ? 0 : head_sep;
			if (dow) {
				printf("%s ", (julian) ?
				    j_day_headings + 4 * dow :
				    day_headings + 3 * dow);
				printf("%.*s", dow * (julian ? 4 : 3) - 1,
				       (julian) ? j_day_headings : day_headings);
			} else
				printf("%s", (julian) ? j_day_headings : day_headings);
			printf("%*s", sep, "");
		}
		printf("\n");

		for (row = 0; row < 6; row++) {
			char *p = NULL;

			memset(lineout, ' ', sizeof(lineout));
			for (i = 0; i < skip; i++) {
				/* nothing */
			}
			w_off = 0;
			for (; i < month_per_row; i++) {
				int col, *dp;

				if (year == endyear && month + i > endmonth)
					break;

				p = lineout + i * (week_len + 2) + w_off;
				dp = &days[i][row * 7];
				for (col = 0; col < 7;
				     col++, p += day_len + r_off) {
					r_off = ascii_day(p, *dp++);
					w_off += r_off;
				}
			}
			*p = '\0';
			trim_trailing_spaces(lineout);
			(void)printf("%s\n", lineout);
		}

		skip = 0;
		month += month_per_row;
		if (month >= 12) {
			month -= 12;
			year++;
			newyear = 1;
		}
	} while (year < endyear || (year == endyear && month <= endmonth));
}

/*
 * day_array --
 *	Fill in an array of 42 integers with a calendar.  Assume for a moment
 *	that you took the (maximum) 6 rows in a calendar and stretched them
 *	out end to end.  You would have 42 numbers or spaces.  This routine
 *	builds that array for any month from Jan. 1 through Dec. 9999.
 */
static void
day_array(int month, int year, int *days)
{
	int day, dw, dm;
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	tm->tm_year += TM_YEAR_BASE;
	tm->tm_mon++;
	tm->tm_yday++; /* jan 1 is 1 for us, not 0 */

	for (dm = month + year * 12, dw = 0; dw < 4; dw++) {
		if (dm == shift_days[julian][dw][MAXDAYS]) {
			memmove(days, shift_days[julian][dw],
				MAXDAYS * sizeof(int));
			return;
		}
	}

	memmove(days, empty, MAXDAYS * sizeof(int));
	dm = days_in_month[leap_year(year)][month];
	dw = day_in_week(1, month, year);
	day = julian ? day_in_year(1, month, year) : 1;
	while (dm--) {
		if (hilite && year == tm->tm_year &&
		    (julian ? (day == tm->tm_yday) :
		     (month == tm->tm_mon && day == tm->tm_mday)))
			days[dw++] = SPACE - day++;
		else
			days[dw++] = day++;
	}
}

/*
 * day_in_year --
 *	return the 1 based day number within the year
 */
static int
day_in_year(int day, int month, int year)
{
	int i, leap;

	leap = leap_year(year);
	for (i = 1; i < month; i++)
		day += days_in_month[leap][i];
	return (day);
}

/*
 * day_in_week
 *	return the 0 based day number for any date from 1 Jan. 1 to
 *	31 Dec. 9999.  Returns the day of the week of the first
 *	missing day for any given Gregorian shift.
 */
static int
day_in_week(int day, int month, int year)
{
	long temp;

	temp = (long)(year - 1) * 365 + leap_years_since_year_1(year - 1)
	    + day_in_year(day, month, year);
	if (temp < FIRST_MISSING_DAY)
		return ((temp - dow + 6 + SATURDAY) % 7);
	if (temp >= (FIRST_MISSING_DAY + NUMBER_MISSING_DAYS))
		return (((temp - dow + 6 + SATURDAY) - NUMBER_MISSING_DAYS) % 7);
	return ((FIRST_MISSING_DAY - dow + 6 + SATURDAY) % 7);
}

static int
ascii_day(char *p, int day)
{
	int display, val, rc;
	char *b;
	static const char *aday[] = {
		"",
		" 1", " 2", " 3", " 4", " 5", " 6", " 7",
		" 8", " 9", "10", "11", "12", "13", "14",
		"15", "16", "17", "18", "19", "20", "21",
		"22", "23", "24", "25", "26", "27", "28",
		"29", "30", "31",
	};

	if (day == SPACE) {
		memset(p, ' ', julian ? J_DAY_LEN : DAY_LEN);
		return (0);
	}
	if (day < SPACE) {
		b = p;
		day = SPACE - day;
	} else
		b = NULL;
	if (julian) {
		if ((val = day / 100) != 0) {
			day %= 100;
			*p++ = val + '0';
			display = 1;
		} else {
			*p++ = ' ';
			display = 0;
		}
		val = day / 10;
		if (val || display)
			*p++ = val + '0';
		else
			*p++ = ' ';
		*p++ = day % 10 + '0';
	} else {
		*p++ = aday[day][0];
		*p++ = aday[day][1];
	}

	rc = 0;
	if (b != NULL) {
		const char *t;
		char h[64];
		int l;

		l = p - b;
		memcpy(h, b, l);
		p = b;

		if (md != NULL) {
			for (t = md; *t; rc++)
				*p++ = *t++;
			memcpy(p, h, l);
			p += l;
			for (t = me; *t; rc++)
				*p++ = *t++;
		} else {
			for (t = &h[0]; l--; t++) {
				*p++ = *t;
				rc++;
				*p++ = '\b';
				rc++;
				*p++ = *t;
			}
		}
	}

	*p = ' ';
	return (rc);
}

static void
trim_trailing_spaces(char *s)
{
	char *p;

	for (p = s; *p; ++p)
		continue;
	while (p > s && isspace((unsigned char)*--p))
		continue;
	if (p > s)
		++p;
	*p = '\0';
}

static void
center(const char *str, int len, int separate)
{

	len -= strlen(str);
	(void)printf("%*s%s%*s", len / 2, "", str, len / 2 + len % 2, "");
	if (separate)
		(void)printf("%*s", separate, "");
}

/*
 * gregorian_reform --
 *	Given a description of date on which the Gregorian Reform was
 *	applied.  The argument can be any of the "country" names
 *	listed in the reforms array (case insensitive) or a date of
 *	the form YYYY/MM/DD.  The date and month can be omitted if
 *	doing so would not select more than one different built-in
 *	reform point.
 */
static void
gregorian_reform(const char *p)
{
	int year, month, date;
	int i, days, diw, diy;
	char c;

	i = sscanf(p, "%d%*[/,-]%d%*[/,-]%d%c", &year, &month, &date, &c);
	switch (i) {
	case 4:
		/*
		 * If the character was sscanf()ed, then there's more
		 * stuff than we need.
		 */
		errx(1, "date specifier %s invalid", p);
	case 0:
		/*
		 * Not a form we can sscanf(), so void these, and we
		 * can try matching "country" names later.
		 */
		year = month = date = -1;
		break;
	case 1:
		month = 0;
		/*FALLTHROUGH*/
	case 2:
		date = 0;
		/*FALLTHROUGH*/
	    case 3:
		/*
		 * At last, some sanity checking on the values we were
		 * given.
		 */
		if (year < 1 || year > 9999)
			errx(1, "%d: illegal year value: use 1-9999", year);
		if (i > 1 && (month < 1 || month > 12))
			errx(1, "%d: illegal month value: use 1-12", month);
		if ((i == 3 && date < 1) || date < 0 ||
		    date > days_in_month[1][month])
			/*
			 * What about someone specifying a leap day in
			 * a non-leap year?  Well...that's a tricky
			 * one.  We can't yet *say* whether the year
			 * in question is a leap year.  What if the
			 * date given was, for example, 1700/2/29?  is
			 * that a valid leap day?
			 *
			 * So...we punt, and hope that saying 29 in
			 * the case of February isn't too bad an idea.
			 */
			errx(1, "%d: illegal date value: use 1-%d", date,
			     days_in_month[1][month]);
		break;
	}

	/*
	 * A complete date was specified, so use the other pope.
	 */
	if (date > 0) {
		static struct reform Goestheveezl;

		reform = &Goestheveezl;
		reform->country = "Bompzidaize";
		reform->year = year;
		reform->month = month;
		reform->date = date;
	}

	/*
	 * No date information was specified, so let's try to match on
	 * country name.
	 */
	else if (year == -1) {
		for (reform = &reforms[0]; reform->year; reform++) {
			if (strcasecmp(p, reform->country) == 0)
				break;
		}
	}

	/*
	 * We have *some* date information, but not a complete date.
	 * Let's see if we have enough to pick a single entry from the
	 * list that's not ambiguous.
	 */
	else {
		for (reform = &reforms[0]; reform->year; reform++) {
			if ((year == 0 || year == reform->year) &&
			    (month == 0 || month == reform->month) &&
			    (date == 0 || month == reform->date))
				break;
		}

		if (i <= reform->ambiguity)
			errx(1, "%s: ambiguous short reform date specification", p);
	}

	/*
	 * Oops...we reached the end of the list.
	 */
	if (reform->year == 0)
		errx(1, "reform name %s invalid", p);

	/*
	 * 
	 */
	reform->missing_days =
		j_leap_days(reform->year, reform->month, reform->date) -
		g_leap_days(reform->year, reform->month, reform->date) -
		GREGORIAN_MAGIC;

	reform->first_missing_day =
		(reform->year - 1) * 365 +
		day_in_year(reform->date, reform->month, reform->year) +
		date +
		j_leap_days(reform->year, reform->month, reform->date);

	/*
	 * Once we know the day of the week of the first missing day,
	 * skip back to the first of the month's day of the week.
	 */
	diw = day_in_week(reform->date, reform->month, reform->year);
	diw = (diw + 8 - (reform->date % 7)) % 7;
	diy = day_in_year(1, reform->month, reform->year);

	/*
	 * We might need all four of these (if you switch from Julian
	 * to Gregorian at some point after 9900, you get a gap of 73
	 * days, and that can affect four months), and it doesn't hurt
	 * all that much to precompute them, so there.
	 */
	date = 1;
	days = 0;
	for (i = 0; i < 4; i++)
		reform_day_array(reform->month + i, reform->year,
				 &days, &date, &diw, &diy,
				 shift_days[0][i],
				 shift_days[1][i]);
}

/*
 * reform_day_array --
 *	Pre-calculates the given month's calendar (in both "standard"
 *	and "julian day" representations) with respect for days
 *	skipped during a reform period.
 */
static void
reform_day_array(int month, int year, int *done, int *date, int *diw, int *diy,
	int *scal, int *jcal)
{
	int mdays;

	/*
	 * If the reform was in the month of october or later, then
	 * the month number from the caller could "overflow".
	 */
	if (month > 12) {
		month -= 12;
		year++;
	}

	/*
	 * Erase months, and set crib number.  The crib number is used
	 * later to determine if the month to be displayed is here or
	 * should be built on the fly with the generic routine
	 */
	memmove(scal, empty, MAXDAYS * sizeof(int));
	scal[MAXDAYS] = month + year * 12;
	memmove(jcal, empty, MAXDAYS * sizeof(int));
	jcal[MAXDAYS] = month + year * 12;

	/*
	 * It doesn't matter what the actual month is when figuring
	 * out if this is a leap year or not, just so long as February
	 * gets the right number of days in it.
	 */
	mdays = days_in_month[g_leap_year(year, 3, 1)][month];

	/*
	 * Bounce back to the first "row" in the day array, and fill
	 * in any days that actually occur.
	 */
	for (*diw %= 7; (*date - *done) <= mdays; (*date)++, (*diy)++) {
		/*
		 * "date" doesn't get reset by the caller across calls
		 * to this routine, so we can actually tell that we're
		 * looking at April the 41st.  Much easier than trying
		 * to calculate the absolute julian day for a given
		 * date and then checking that.
		 */
		if (*date < reform->date ||
		    *date >= reform->date + reform->missing_days) {
			scal[*diw] = *date - *done;
			jcal[*diw] = *diy;
			(*diw)++;
		}
	}
	*done += mdays;
}

static int
getnum(const char *p)
{
	unsigned long result;
	char *ep;

	errno = 0;
	result = strtoul(p, &ep, 10);
	if (p[0] == '\0' || *ep != '\0')
		goto error;
	if (errno == ERANGE && result == ULONG_MAX)
		goto error;
	if (result > INT_MAX)
		goto error;

	return (int)result;

error:
	errx(1, "bad number: %s", p);
	/*NOTREACHED*/
}

static void
init_hilite(void)
{
	const char *term;
	int errret;

	hilite++;

	if (!isatty(fileno(stdout)))
		return;

	term = getenv("TERM");
	if (term == NULL)
		term = "dumb";
	if (setupterm(term, fileno(stdout), &errret) != 0 && errret != 1)
		return;

	if (hilite > 1)
		md = enter_reverse_mode;
	else
		md = enter_bold_mode;
	me = exit_attribute_mode;
	if (me == NULL || md == NULL)
		md = me = NULL;
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: cal [-3hjry] [-A after] [-B before] [-d day-of-week] "
	    "[-R reform-spec]\n           [[month] year]\n");
	exit(1);
}
