/*	$NetBSD: strftime.c,v 1.29 2013/09/20 19:06:54 christos Exp $	*/

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char	elsieid[] = "@(#)strftime.c	7.64";
static char	elsieid[] = "@(#)strftime.c	8.3";
#else
__RCSID("$NetBSD: strftime.c,v 1.29 2013/09/20 19:06:54 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <stddef.h>
#include <locale.h>
#include "setlocale_local.h"

/*
** Based on the UCB version with the copyright notice and sccsid
** appearing below.
**
** This is ANSIish only when "multibyte character == plain character".
*/

#include "private.h"

/*
** We don't use these extensions in strftime operation even when
** supported by the local tzcode configuration.  A strictly
** conforming C application may leave them in undefined state.
*/

#ifdef _LIBC
#undef TM_ZONE
#undef TM_GMTOFF
#endif

/*
** Copyright (c) 1989, 1993
**	The Regents of the University of California.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. All advertising materials mentioning features or use of this software
**    must display the following acknowledgement:
**	This product includes software developed by the University of
**	California, Berkeley and its contributors.
** 4. Neither the name of the University nor the names of its contributors
**    may be used to endorse or promote products derived from this software
**    without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/

#ifndef LIBC_SCCS
#ifndef lint
static const char	sccsid[] = "@(#)strftime.c	5.4 (Berkeley) 3/14/89";
#endif /* !defined lint */
#endif /* !defined LIBC_SCCS */

#include "tzfile.h"
#include "fcntl.h"
#include "locale.h"

#ifdef __weak_alias
__weak_alias(strftime_l, _strftime_l)
__weak_alias(strftime_lz, _strftime_lz)
__weak_alias(strftime_z, _strftime_z)
#endif

#include "sys/localedef.h"
#define _TIME_LOCALE(loc) \
    ((_TimeLocale *)((loc)->part_impl[(size_t)LC_TIME]))
#define c_fmt   d_t_fmt

static char *	_add(const char *, char *, const char *);
static char *	_conv(int, const char *, char *, const char *);
static char *	_fmt(const timezone_t, const char *, const struct tm *, char *,
			const char *, int *, locale_t);
static char *	_yconv(int, int, int, int, char *, const char *);

extern char *	tzname[];

#ifndef YEAR_2000_NAME
#define YEAR_2000_NAME	"CHECK_STRFTIME_FORMATS_FOR_TWO_DIGIT_YEARS"
#endif /* !defined YEAR_2000_NAME */

#define IN_NONE	0
#define IN_SOME	1
#define IN_THIS	2
#define IN_ALL	3

size_t
strftime_z(const timezone_t sp, char * __restrict s, size_t maxsize,
    const char * __restrict format, const struct tm * __restrict t)
{
	return strftime_lz(sp, s, maxsize, format, t, _current_locale());
}

size_t
strftime_lz(const timezone_t sp, char *const s, const size_t maxsize,
    const char *const format, const struct tm *const t, locale_t loc)
{
	char *	p;
	int	warn;

	warn = IN_NONE;
	p = _fmt(sp, ((format == NULL) ? "%c" : format), t, s, s + maxsize,
	    &warn, loc);
#ifndef NO_RUN_TIME_WARNINGS_ABOUT_YEAR_2000_PROBLEMS_THANK_YOU
	if (warn != IN_NONE && getenv(YEAR_2000_NAME) != NULL) {
		(void) fprintf(stderr, "\n");
		if (format == NULL)
			(void) fprintf(stderr, "NULL strftime format ");
		else	(void) fprintf(stderr, "strftime format \"%s\" ",
				format);
		(void) fprintf(stderr, "yields only two digits of years in ");
		if (warn == IN_SOME)
			(void) fprintf(stderr, "some locales");
		else if (warn == IN_THIS)
			(void) fprintf(stderr, "the current locale");
		else	(void) fprintf(stderr, "all locales");
		(void) fprintf(stderr, "\n");
	}
#endif /* !defined NO_RUN_TIME_WARNINGS_ABOUT_YEAR_2000_PROBLEMS_THANK_YOU */
	if (p == s + maxsize)
		return 0;
	*p = '\0';
	return p - s;
}

static char *
_fmt(const timezone_t sp, const char *format, const struct tm *const t,
	char *pt, const char *const ptlim, int *warnp, locale_t loc)
{
	for ( ; *format; ++format) {
		if (*format == '%') {
label:
			switch (*++format) {
			case '\0':
				--format;
				break;
			case 'A':
				pt = _add((t->tm_wday < 0 ||
					t->tm_wday >= DAYSPERWEEK) ?
					"?" : _TIME_LOCALE(loc)->day[t->tm_wday],
					pt, ptlim);
				continue;
			case 'a':
				pt = _add((t->tm_wday < 0 ||
					t->tm_wday >= DAYSPERWEEK) ?
					"?" : _TIME_LOCALE(loc)->abday[t->tm_wday],
					pt, ptlim);
				continue;
			case 'B':
				pt = _add((t->tm_mon < 0 ||
					t->tm_mon >= MONSPERYEAR) ?
					"?" : _TIME_LOCALE(loc)->mon[t->tm_mon],
					pt, ptlim);
				continue;
			case 'b':
			case 'h':
				pt = _add((t->tm_mon < 0 ||
					t->tm_mon >= MONSPERYEAR) ?
					"?" : _TIME_LOCALE(loc)->abmon[t->tm_mon],
					pt, ptlim);
				continue;
			case 'C':
				/*
				** %C used to do a...
				**	_fmt("%a %b %e %X %Y", t);
				** ...whereas now POSIX 1003.2 calls for
				** something completely different.
				** (ado, 1993-05-24)
				*/
				pt = _yconv(t->tm_year, TM_YEAR_BASE, 1, 0,
					pt, ptlim);
				continue;
			case 'c':
				{
				int warn2 = IN_SOME;

				pt = _fmt(sp, _TIME_LOCALE(loc)->c_fmt, t, pt,
				    ptlim, &warn2, loc);
				if (warn2 == IN_ALL)
					warn2 = IN_THIS;
				if (warn2 > *warnp)
					*warnp = warn2;
				}
				continue;
			case 'D':
				pt = _fmt(sp, "%m/%d/%y", t, pt, ptlim, warnp,
				    loc);
				continue;
			case 'd':
				pt = _conv(t->tm_mday, "%02d", pt, ptlim);
				continue;
			case 'E':
			case 'O':
				/*
				** C99 locale modifiers.
				** The sequences
				**	%Ec %EC %Ex %EX %Ey %EY
				**	%Od %oe %OH %OI %Om %OM
				**	%OS %Ou %OU %OV %Ow %OW %Oy
				** are supposed to provide alternate
				** representations.
				*/
				goto label;
			case 'e':
				pt = _conv(t->tm_mday, "%2d", pt, ptlim);
				continue;
			case 'F':
				pt = _fmt(sp, "%Y-%m-%d", t, pt, ptlim, warnp,
				    loc);
				continue;
			case 'H':
				pt = _conv(t->tm_hour, "%02d", pt, ptlim);
				continue;
			case 'I':
				pt = _conv((t->tm_hour % 12) ?
					(t->tm_hour % 12) : 12,
					"%02d", pt, ptlim);
				continue;
			case 'j':
				pt = _conv(t->tm_yday + 1, "%03d", pt, ptlim);
				continue;
			case 'k':
				/*
				** This used to be...
				**	_conv(t->tm_hour % 12 ?
				**		t->tm_hour % 12 : 12, 2, ' ');
				** ...and has been changed to the below to
				** match SunOS 4.1.1 and Arnold Robbins'
				** strftime version 3.0. That is, "%k" and
				** "%l" have been swapped.
				** (ado, 1993-05-24)
				*/
				pt = _conv(t->tm_hour, "%2d", pt, ptlim);
				continue;
#ifdef KITCHEN_SINK
			case 'K':
				/*
				** After all this time, still unclaimed!
				*/
				pt = _add("kitchen sink", pt, ptlim);
				continue;
#endif /* defined KITCHEN_SINK */
			case 'l':
				/*
				** This used to be...
				**	_conv(t->tm_hour, 2, ' ');
				** ...and has been changed to the below to
				** match SunOS 4.1.1 and Arnold Robbin's
				** strftime version 3.0. That is, "%k" and
				** "%l" have been swapped.
				** (ado, 1993-05-24)
				*/
				pt = _conv((t->tm_hour % 12) ?
					(t->tm_hour % 12) : 12,
					"%2d", pt, ptlim);
				continue;
			case 'M':
				pt = _conv(t->tm_min, "%02d", pt, ptlim);
				continue;
			case 'm':
				pt = _conv(t->tm_mon + 1, "%02d", pt, ptlim);
				continue;
			case 'n':
				pt = _add("\n", pt, ptlim);
				continue;
			case 'p':
				pt = _add((t->tm_hour >= (HOURSPERDAY / 2)) ?
					_TIME_LOCALE(loc)->am_pm[1] :
					_TIME_LOCALE(loc)->am_pm[0],
					pt, ptlim);
				continue;
			case 'R':
				pt = _fmt(sp, "%H:%M", t, pt, ptlim, warnp,
				    loc);
				continue;
			case 'r':
				pt = _fmt(sp, _TIME_LOCALE(loc)->t_fmt_ampm, t,
				    pt, ptlim, warnp, loc);
				continue;
			case 'S':
				pt = _conv(t->tm_sec, "%02d", pt, ptlim);
				continue;
			case 's':
				{
					struct tm	tm;
					char		buf[INT_STRLEN_MAXIMUM(
								time_t) + 1];
					time_t		mkt;

					tm = *t;
					mkt = mktime(&tm);
					/* CONSTCOND */
					if (TYPE_SIGNED(time_t))
						(void)snprintf(buf, sizeof(buf),
						    "%jd", (intmax_t) mkt);
					else	(void)snprintf(buf, sizeof(buf),
						    "%ju", (uintmax_t) mkt);
					pt = _add(buf, pt, ptlim);
				}
				continue;
			case 'T':
				pt = _fmt(sp, "%H:%M:%S", t, pt, ptlim, warnp,
				    loc);
				continue;
			case 't':
				pt = _add("\t", pt, ptlim);
				continue;
			case 'U':
				pt = _conv((t->tm_yday + DAYSPERWEEK -
					t->tm_wday) / DAYSPERWEEK,
					"%02d", pt, ptlim);
				continue;
			case 'u':
				/*
				** From Arnold Robbins' strftime version 3.0:
				** "ISO 8601: Weekday as a decimal number
				** [1 (Monday) - 7]"
				** (ado, 1993-05-24)
				*/
				pt = _conv((t->tm_wday == 0) ?
					DAYSPERWEEK : t->tm_wday,
					"%d", pt, ptlim);
				continue;
			case 'V':	/* ISO 8601 week number */
			case 'G':	/* ISO 8601 year (four digits) */
			case 'g':	/* ISO 8601 year (two digits) */
/*
** From Arnold Robbins' strftime version 3.0: "the week number of the
** year (the first Monday as the first day of week 1) as a decimal number
** (01-53)."
** (ado, 1993-05-24)
**
** From "http://www.ft.uni-erlangen.de/~mskuhn/iso-time.html" by Markus Kuhn:
** "Week 01 of a year is per definition the first week which has the
** Thursday in this year, which is equivalent to the week which contains
** the fourth day of January. In other words, the first week of a new year
** is the week which has the majority of its days in the new year. Week 01
** might also contain days from the previous year and the week before week
** 01 of a year is the last week (52 or 53) of the previous year even if
** it contains days from the new year. A week starts with Monday (day 1)
** and ends with Sunday (day 7). For example, the first week of the year
** 1997 lasts from 1996-12-30 to 1997-01-05..."
** (ado, 1996-01-02)
*/
				{
					int	year;
					int	base;
					int	yday;
					int	wday;
					int	w;

					year = t->tm_year;
					base = TM_YEAR_BASE;
					yday = t->tm_yday;
					wday = t->tm_wday;
					for ( ; ; ) {
						int	len;
						int	bot;
						int	top;

						len = isleap_sum(year, base) ?
							DAYSPERLYEAR :
							DAYSPERNYEAR;
						/*
						** What yday (-3 ... 3) does
						** the ISO year begin on?
						*/
						bot = ((yday + 11 - wday) %
							DAYSPERWEEK) - 3;
						/*
						** What yday does the NEXT
						** ISO year begin on?
						*/
						top = bot -
							(len % DAYSPERWEEK);
						if (top < -3)
							top += DAYSPERWEEK;
						top += len;
						if (yday >= top) {
							++base;
							w = 1;
							break;
						}
						if (yday >= bot) {
							w = 1 + ((yday - bot) /
								DAYSPERWEEK);
							break;
						}
						--base;
						yday += isleap_sum(year, base) ?
							DAYSPERLYEAR :
							DAYSPERNYEAR;
					}
#ifdef XPG4_1994_04_09
					if ((w == 52 &&
						t->tm_mon == TM_JANUARY) ||
						(w == 1 &&
						t->tm_mon == TM_DECEMBER))
							w = 53;
#endif /* defined XPG4_1994_04_09 */
					if (*format == 'V')
						pt = _conv(w, "%02d",
							pt, ptlim);
					else if (*format == 'g') {
						*warnp = IN_ALL;
						pt = _yconv(year, base, 0, 1,
							pt, ptlim);
					} else	pt = _yconv(year, base, 1, 1,
							pt, ptlim);
				}
				continue;
			case 'v':
				/*
				** From Arnold Robbins' strftime version 3.0:
				** "date as dd-bbb-YYYY"
				** (ado, 1993-05-24)
				*/
				pt = _fmt(sp, "%e-%b-%Y", t, pt, ptlim, warnp,
				    loc);
				continue;
			case 'W':
				pt = _conv((t->tm_yday + DAYSPERWEEK -
					(t->tm_wday ?
					(t->tm_wday - 1) :
					(DAYSPERWEEK - 1))) / DAYSPERWEEK,
					"%02d", pt, ptlim);
				continue;
			case 'w':
				pt = _conv(t->tm_wday, "%d", pt, ptlim);
				continue;
			case 'X':
				pt = _fmt(sp, _TIME_LOCALE(loc)->t_fmt, t, pt,
				    ptlim, warnp, loc);
				continue;
			case 'x':
				{
				int	warn2 = IN_SOME;

				pt = _fmt(sp, _TIME_LOCALE(loc)->d_fmt, t, pt,
				    ptlim, &warn2, loc);
				if (warn2 == IN_ALL)
					warn2 = IN_THIS;
				if (warn2 > *warnp)
					*warnp = warn2;
				}
				continue;
			case 'y':
				*warnp = IN_ALL;
				pt = _yconv(t->tm_year, TM_YEAR_BASE, 0, 1,
					pt, ptlim);
				continue;
			case 'Y':
				pt = _yconv(t->tm_year, TM_YEAR_BASE, 1, 1,
					pt, ptlim);
				continue;
			case 'Z':
#ifdef TM_ZONE
				if (t->TM_ZONE != NULL)
					pt = _add(t->TM_ZONE, pt, ptlim);
				else
#endif /* defined TM_ZONE */
				if (t->tm_isdst >= 0)
					pt = _add((sp ?
					    tzgetname(sp, t->tm_isdst) :
					    tzname[t->tm_isdst != 0]),
					    pt, ptlim);
				/*
				** C99 says that %Z must be replaced by the
				** empty string if the time zone is not
				** determinable.
				*/
				continue;
			case 'z':
				{
				int		diff;
				char const *	sign;

				if (t->tm_isdst < 0)
					continue;
#ifdef TM_GMTOFF
				diff = (int)t->TM_GMTOFF;
#else /* !defined TM_GMTOFF */
				/*
				** C99 says that the UT offset must
				** be computed by looking only at
				** tm_isdst. This requirement is
				** incorrect, since it means the code
				** must rely on magic (in this case
				** altzone and timezone), and the
				** magic might not have the correct
				** offset. Doing things correctly is
				** tricky and requires disobeying C99;
				** see GNU C strftime for details.
				** For now, punt and conform to the
				** standard, even though it's incorrect.
				**
				** C99 says that %z must be replaced by the
				** empty string if the time zone is not
				** determinable, so output nothing if the
				** appropriate variables are not available.
				*/
#ifndef STD_INSPIRED
				if (t->tm_isdst == 0)
#ifdef USG_COMPAT
					diff = -timezone;
#else /* !defined USG_COMPAT */
					continue;
#endif /* !defined USG_COMPAT */
				else
#ifdef ALTZONE
					diff = -altzone;
#else /* !defined ALTZONE */
					continue;
#endif /* !defined ALTZONE */
#else /* defined STD_INSPIRED */
				{
					struct tm tmp;
					time_t lct, gct;

					/*
					** Get calendar time from t
					** being treated as local.
					*/
					tmp = *t; /* mktime discards const */
					lct = mktime(&tmp);

					if (lct == (time_t)-1)
						continue;

					/*
					** Get calendar time from t
					** being treated as GMT.
					**/
					tmp = *t; /* mktime discards const */
					gct = timegm(&tmp);

					if (gct == (time_t)-1)
						continue;

					/* LINTED difference will fit int */
					diff = (intmax_t)gct - (intmax_t)lct;
				}
#endif /* defined STD_INSPIRED */
#endif /* !defined TM_GMTOFF */
				if (diff < 0) {
					sign = "-";
					diff = -diff;
				} else	sign = "+";
				pt = _add(sign, pt, ptlim);
				diff /= SECSPERMIN;
				diff = (diff / MINSPERHOUR) * 100 +
					(diff % MINSPERHOUR);
				pt = _conv(diff, "%04d", pt, ptlim);
				}
				continue;
#if defined(__minix)
			case '+':
				pt = _fmt(sp, _TIME_LOCALE(loc)->c_fmt, t, pt, ptlim, warnp, loc);
				continue;
#endif /* defined(__minix) */
#if 0
			case '+':
				pt = _fmt(sp, _TIME_LOCALE(loc)->date_fmt, t,
				    pt, ptlim, warnp, loc);
				continue;
#endif
			case '%':
			/*
			** X311J/88-090 (4.12.3.5): if conversion char is
			** undefined, behavior is undefined. Print out the
			** character itself as printf(3) also does.
			*/
			default:
				break;
			}
		}
		if (pt == ptlim)
			break;
		*pt++ = *format;
	}
	return pt;
}

size_t
strftime(char * const s, const size_t maxsize,
    const char * const format, const struct tm * const	t)
{
	tzset();
	return strftime_z(NULL, s, maxsize, format, t);
}

size_t
strftime_l(char * __restrict s, size_t maxsize, const char * __restrict format,
    const struct tm * __restrict t, locale_t loc)
{
	tzset();
	return strftime_lz(NULL, s, maxsize, format, t, loc);
}

static char *
_conv(const int	n, const char *const format, char *const pt,
    const char *const ptlim)
{
	char	buf[INT_STRLEN_MAXIMUM(int) + 1];

	(void) snprintf(buf, sizeof(buf), format, n);
	return _add(buf, pt, ptlim);
}

static char *
_add(const char *str, char *pt, const char *const ptlim)
{
	while (pt < ptlim && (*pt = *str++) != '\0')
		++pt;
	return pt;
}

/*
** POSIX and the C Standard are unclear or inconsistent about
** what %C and %y do if the year is negative or exceeds 9999.
** Use the convention that %C concatenated with %y yields the
** same output as %Y, and that %Y contains at least 4 bytes,
** with more only if necessary.
*/

static char *
_yconv(const int a, const int b, const int convert_top, const int convert_yy,
    char *pt, const char *const ptlim)
{
	int	lead;
	int	trail;

#define DIVISOR	100
	trail = a % DIVISOR + b % DIVISOR;
	lead = a / DIVISOR + b / DIVISOR + trail / DIVISOR;
	trail %= DIVISOR;
	if (trail < 0 && lead > 0) {
		trail += DIVISOR;
		--lead;
	} else if (lead < 0 && trail > 0) {
		trail -= DIVISOR;
		++lead;
	}
	if (convert_top) {
		if (lead == 0 && trail < 0)
			pt = _add("-0", pt, ptlim);
		else	pt = _conv(lead, "%02d", pt, ptlim);
	}
	if (convert_yy)
		pt = _conv(((trail < 0) ? -trail : trail), "%02d", pt, ptlim);
	return pt;
}

#ifdef LOCALE_HOME
static struct lc_time_T *
_loc(void)
{
	static const char	locale_home[] = LOCALE_HOME;
	static const char	lc_time[] = "LC_TIME";
	static char *		locale_buf;

	int			fd;
	int			oldsun;	/* "...ain't got nothin' to do..." */
	char *			lbuf;
	char *			name;
	char *			p;
	const char **		ap;
	const char *		plim;
	char			filename[FILENAME_MAX];
	struct stat		st;
	size_t			namesize;
	size_t			bufsize;

	/*
	** Use localebuf.mon[0] to signal whether locale is already set up.
	*/
	if (localebuf.mon[0])
		return &localebuf;
	name = setlocale(LC_TIME, NULL);
	if (name == NULL || *name == '\0')
		goto no_locale;
	/*
	** If the locale name is the same as our cache, use the cache.
	*/
	lbuf = locale_buf;
	if (lbuf != NULL && strcmp(name, lbuf) == 0) {
		p = lbuf;
		for (ap = (const char **) &localebuf;
			ap < (const char **) (&localebuf + 1);
				++ap)
					*ap = p += strlen(p) + 1;
		return &localebuf;
	}
	/*
	** Slurp the locale file into the cache.
	*/
	namesize = strlen(name) + 1;
	if (sizeof filename <
		((sizeof locale_home) + namesize + (sizeof lc_time)))
			goto no_locale;
	oldsun = 0;
	(void) sprintf(filename, "%s/%s/%s", locale_home, name, lc_time);
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		/*
		** Old Sun systems have a different naming and data convention.
		*/
		oldsun = 1;
		(void) sprintf(filename, "%s/%s/%s", locale_home,
			lc_time, name);
		fd = open(filename, O_RDONLY);
		if (fd < 0)
			goto no_locale;
	}
	if (fstat(fd, &st) != 0)
		goto bad_locale;
	if (st.st_size <= 0)
		goto bad_locale;
	bufsize = namesize + st.st_size;
	locale_buf = NULL;
	lbuf = (lbuf == NULL) ? malloc(bufsize) : realloc(lbuf, bufsize);
	if (lbuf == NULL)
		goto bad_locale;
	(void) strcpy(lbuf, name);
	p = lbuf + namesize;
	plim = p + st.st_size;
	if (read(fd, p, (size_t) st.st_size) != st.st_size)
		goto bad_lbuf;
	if (close(fd) != 0)
		goto bad_lbuf;
	/*
	** Parse the locale file into localebuf.
	*/
	if (plim[-1] != '\n')
		goto bad_lbuf;
	for (ap = (const char **) &localebuf;
		ap < (const char **) (&localebuf + 1);
			++ap) {
				if (p == plim)
					goto bad_lbuf;
				*ap = p;
				while (*p != '\n')
					++p;
				*p++ = '\0';
	}
	if (oldsun) {
		/*
		** SunOS 4 used an obsolescent format; see localdtconv(3).
		** c_fmt had the ``short format for dates and times together''
		** (SunOS 4 date, "%a %b %e %T %Z %Y" in the C locale);
		** date_fmt had the ``long format for dates''
		** (SunOS 4 strftime %C, "%A, %B %e, %Y" in the C locale).
		** Discard the latter in favor of the former.
		*/
		localebuf.date_fmt = localebuf.c_fmt;
	}
	/*
	** Record the successful parse in the cache.
	*/
	locale_buf = lbuf;

	return &localebuf;

bad_lbuf:
	free(lbuf);
bad_locale:
	(void) close(fd);
no_locale:
	localebuf = C_time_locale;
	locale_buf = NULL;
	return &localebuf;
}
#endif /* defined LOCALE_HOME */
