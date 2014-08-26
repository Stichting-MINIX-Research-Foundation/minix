/*	$NetBSD: pr_time.c,v 1.18 2011/08/17 13:48:11 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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
#if 0
static char sccsid[] = "@(#)pr_time.c	8.2 (Berkeley) 4/4/94";
#else
__RCSID("$NetBSD: pr_time.c,v 1.18 2011/08/17 13:48:11 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>

#include "extern.h"

/*
 * pr_attime --
 *	Print the time since the user logged in.
 *
 *	Note: SCCS forces the bizarre string manipulation, things like
 *	%I% get replaced in the source code.
 */
void
pr_attime(time_t *started, time_t *now)
{
	static char buf[256];
	int tnow_yday;
	struct tm *tp;
	time_t diff;

	tnow_yday = localtime(now)->tm_yday;
	tp = localtime(started);
	diff = *now - *started;

	if (diff > SECSPERDAY * DAYSPERWEEK) {
		/* If more than a week, use day-month-year. */
		(void)strftime(buf, sizeof(buf), "%d%b%y", tp);
	} else if (tp->tm_yday != tnow_yday) {
		/* If not today, use day-hour-am/pm. Damn SCCS */
		(void)strftime(buf, sizeof(buf), "%a%" "I%p", tp);
	} else {
		/* Default is hh:mm{am,pm}. Damn SCCS */
		(void)strftime(buf, sizeof(buf), "%l:%" "M%p", tp);
	}

	buf[sizeof(buf) - 1] = '\0';
	(void)fputs(buf, stdout);
}

/*
 * pr_idle --
 *	Display the idle time.
 */
void
pr_idle(time_t idle)
{
	int days;

	if (idle == (time_t)-1) {
		(void)printf("     ? ");
		return;
	}

	days = idle / SECSPERDAY;

	/* If idle more than 36 hours, print as a number of days. */
	if (idle >= 48 * SECSPERHOUR)
		printf(" %ddays ", days);
	else if (idle >= 36 * SECSPERHOUR)
		printf("  1day ");

	/* If idle more than an hour, print as HH:MM. */
	else if (idle >= SECSPERHOUR)
		(void)printf(" %2d:%02d ",
		    (int)(idle / SECSPERHOUR),
		    (int)((idle % SECSPERHOUR) / SECSPERMIN));

	/* Else print the minutes idle. */
	else
		(void)printf("    %2d ", (int)(idle / SECSPERMIN));
}
