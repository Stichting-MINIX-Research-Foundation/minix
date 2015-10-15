/*	$NetBSD: getdate.c,v 1.3 2014/09/18 13:58:20 christos Exp $	*/
/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Brian Ginsbach.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

#define TMSENTINEL	(-1)

/*
 * getdate_err is set to one of the following values on error.
 *
 * 1	The DATEMSK environment variable is null or undefined.
 * 2	The template file cannot be opened for reading.
 * 3	Failed to get file status information.
 * 4	Template file is not a regular file.
 * 5	Encountered an error while reading the template file.
 * 6	Cannot allocate memory.
 * 7	Input string does not match any line in the template.
 * 8	Input string is invalid (for example, February 31) or could not
 *	be represented in a time_t.
 */

int getdate_err;

struct tm *
getdate(const char *str)
{
	char *datemsk, *line, *rp;
	FILE *fp;
	struct stat sb;
	static struct tm rtm, tmnow;
	struct tm *tmp, *rtmp = &rtm;
	size_t lineno = 0;
	time_t now;

	if (((datemsk = getenv("DATEMSK")) == NULL) || *datemsk == '\0') {
		getdate_err = 1;
		return (NULL);
	}

	if (stat(datemsk, &sb) < 0) {
		getdate_err = 3;
		return (NULL);
	}

	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		getdate_err = 4;
		return (NULL);
	}

	if ((fp = fopen(datemsk, "re")) == NULL) {
		getdate_err = 2;
		return (NULL);
	}

	/* loop through datemsk file */
	errno = 0;
	rp = NULL;
	while ((line = fparseln(fp, NULL, &lineno, NULL, 0)) != NULL) {
		/* initialize tmp with sentinels */
		rtm.tm_sec = rtm.tm_min = rtm.tm_hour = TMSENTINEL;
		rtm.tm_mday = rtm.tm_mon = rtm.tm_year = TMSENTINEL;
		rtm.tm_wday = rtm.tm_yday = rtm.tm_isdst = TMSENTINEL;
		rtm.tm_gmtoff = 0;
		rtm.tm_zone = NULL;
		rp = strptime(str, line, rtmp);
		free(line);
		if (rp != NULL) 
			break;
		errno = 0;
	}
	if (errno != 0 || ferror(fp)) {
		if (errno == ENOMEM)
			getdate_err = 6;
		else
			getdate_err = 5;
		fclose(fp);
		return (NULL);
	}
	if (feof(fp) || (rp != NULL && *rp != '\0')) {
		getdate_err = 7;
		return (NULL);
	}
	fclose(fp);

	time(&now);
	tmp = localtime(&now);
	tmnow = *tmp;

	/*
	 * This implementation does not accept setting the broken-down time
	 * to anything other than the localtime().  It is not possible to
	 * change the scanned timezone with %Z.
	 *
	 * Note IRIX and Solaris accept only the current zone for %Z.
	 * XXX Is there any implementation that matches the standard?
	 * XXX (Or am I reading the standard wrong?)
	 *
	 * Note: Neither XPG 6 (POSIX 2004) nor XPG 7 (POSIX 2008)
	 * requires strptime(3) support for %Z.
	 */

	/*
	 * Given only a weekday find the first matching weekday starting
	 * with the current weekday and moving into the future.
	 */
	if (rtm.tm_wday != TMSENTINEL && rtm.tm_year == TMSENTINEL &&
	    rtm.tm_mon == TMSENTINEL && rtm.tm_mday == TMSENTINEL) {
		rtm.tm_year = tmnow.tm_year;
		rtm.tm_mon = tmnow.tm_mon;
		rtm.tm_mday = tmnow.tm_mday +
			(rtm.tm_wday - tmnow.tm_wday + 7) % 7;
	}

	/*
	 * Given only a month (and no year) find the first matching month
	 * starting with the current month and moving into the future.
	 */
	if (rtm.tm_mon != TMSENTINEL) {
		if (rtm.tm_year == TMSENTINEL) {
			rtm.tm_year = tmnow.tm_year +
				((rtm.tm_mon < tmnow.tm_mon)? 1 : 0);
		}
		if (rtm.tm_mday == TMSENTINEL) {
			/* assume the first of the month */
			rtm.tm_mday = 1;
			/*
			 * XXX This isn't documented! Just observed behavior.
			 *
			 * Given the weekday find the first matching weekday
			 * starting with the weekday of the first day of the
			 * the month and moving into the future.
			 */
			if (rtm.tm_wday != TMSENTINEL) {
				struct tm tm;

				memset(&tm, 0, sizeof(struct tm));
				tm.tm_year = rtm.tm_year;
				tm.tm_mon = rtm.tm_mon;
				tm.tm_mday = 1;
				mktime(&tm);
				rtm.tm_mday +=
					(rtm.tm_wday - tm.tm_wday + 7) % 7;
			}
		}
	}

	/*
	 * Given no time of day assume the current time of day.
	 */
	if (rtm.tm_hour == TMSENTINEL &&
	    rtm.tm_min == TMSENTINEL && rtm.tm_sec == TMSENTINEL) {
		rtm.tm_hour = tmnow.tm_hour;
		rtm.tm_min = tmnow.tm_min;
		rtm.tm_sec = tmnow.tm_sec;
	}
	/*
	 * Given an hour and no date, find the first matching hour starting
	 * with the current hour and moving into the future
	 */
	if (rtm.tm_hour != TMSENTINEL &&
	    rtm.tm_year == TMSENTINEL && rtm.tm_mon == TMSENTINEL &&
	    rtm.tm_mday == TMSENTINEL) {
		rtm.tm_year = tmnow.tm_year;
		rtm.tm_mon = tmnow.tm_mon;
		rtm.tm_mday = tmnow.tm_mday;
		if (rtm.tm_hour < tmnow.tm_hour)
			rtm.tm_hour += 24;
	}

	/*
	 * Set to 'sane' values; mktime(3) does funny things otherwise.
	 * No hours, no minutes, no seconds, no service.
	 */
	if (rtm.tm_hour == TMSENTINEL)
		rtm.tm_hour = 0;
	if (rtm.tm_min == TMSENTINEL)
		rtm.tm_min = 0;
	if (rtm.tm_sec == TMSENTINEL)
		rtm.tm_sec = 0;

	/*
	 * Given only a year the values of month, day of month, day of year,
	 * week day and is daylight (summer) time are unspecified.
	 * (Specified on the Solaris man page not POSIX.)
	 */
	if (rtm.tm_year != TMSENTINEL &&
	    rtm.tm_mon == TMSENTINEL && rtm.tm_mday == TMSENTINEL) {
		rtm.tm_mon = 0;
		rtm.tm_mday = 1;
		/*
		 * XXX More undocumented functionality but observed.
		 *
		 * Given the weekday find the first matching weekday
		 * starting with the weekday of the first day of the
		 * month and moving into the future.
		 */
		if (rtm.tm_wday != TMSENTINEL) {
			struct tm tm;

			memset(&tm, 0, sizeof(struct tm));
			tm.tm_year = rtm.tm_year;
			tm.tm_mon = rtm.tm_mon;
			tm.tm_mday = 1;
			mktime(&tm);
			rtm.tm_mday += (rtm.tm_wday - tm.tm_wday + 7) % 7;
		}
	}

	/*
	 * Given only the century but no year within, the current year
	 * is assumed.  (Specified on the Solaris man page not POSIX.)
	 *
	 * Warning ugly end case
	 *
	 * This is more work since strptime(3) doesn't "do the right thing".
	 */
	if (rtm.tm_year != TMSENTINEL && (rtm.tm_year - 1900) >= 0) {
		rtm.tm_year -= 1900;
		rtm.tm_year += (tmnow.tm_year % 100);
	}

	/*
	 * mktime() will normalize all values and also check that the
	 * value will fit into a time_t.
	 *
	 * This is only for POSIX correctness.	A date >= 1900 is
	 * really ok, but using a time_t limits things.
	 */
	if (mktime(rtmp) < 0) {
		getdate_err = 8;
		return (NULL);
	}

	return (rtmp);
}
