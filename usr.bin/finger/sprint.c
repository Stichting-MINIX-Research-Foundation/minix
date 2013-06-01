/*	$NetBSD: sprint.c,v 1.17 2006/01/04 01:17:54 perry Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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
static char sccsid[] = "@(#)sprint.c	8.3 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: sprint.c,v 1.17 2006/01/04 01:17:54 perry Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>

#include <time.h>
#include <tzfile.h>
#include <db.h>
#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utmpentry.h"

#include "finger.h"
#include "extern.h"

static void	  stimeprint(WHERE *);

void
sflag_print(void)
{
	PERSON *pn;
	WHERE *w;
	int sflag, r;
	char *p;
	PERSON *tmp;
	DBT data, key;

	if (db == NULL)
		return;

	/*
	 * short format --
	 *	login name
	 *	real name
	 *	terminal name
	 *	if terminal writable (add an '*' to the terminal name
	 *		if not)
	 *	if logged in show idle time and day logged in, else
	 *		show last login date and time.  If > 6 months,
	 *		show year instead of time.  If < 6 days,
	 *		show day name instead of month & day.
	 *	if -h given
	 *		remote host
	 *	else if -o given (overriding -h) (default)
	 *		office location
	 *		office phone
	 */
#define	MAXREALNAME	18
	(void)printf("%-*s %-*s %s %s\n", maxname, "Login", MAXREALNAME,
	    "Name", " Tty      Idle  Login Time  ", (gflag) ? "" :
	    (oflag) ? "Office     Office Phone" : "Where");

	for (sflag = R_FIRST;; sflag = R_NEXT) {
		r = (*db->seq)(db, &key, &data, sflag);
		if (r == -1)
			err(1, "db seq");
		if (r == 1)
			break;
		memmove(&tmp, data.data, sizeof tmp);
		pn = tmp;

		for (w = pn->whead; w != NULL; w = w->next) {
			(void)printf("%-*.*s %-*.*s ", (int)maxname, 
			    (int)maxname,
			    pn->name, MAXREALNAME, MAXREALNAME,
			    pn->realname ? pn->realname : "");
			if (!w->loginat) {
				(void)printf("  *     *  No logins   ");
				goto office;
			}
			(void)putchar(w->info == LOGGEDIN && !w->writable ?
			    '*' : ' ');
			if (*w->tty)
				(void)printf("%-7.7s ", w->tty);
			else
				(void)printf("        ");
			if (w->info == LOGGEDIN) {
				stimeprint(w);
				(void)printf("  ");
			} else
				(void)printf("    *  ");
			p = ctime(&w->loginat);
			if (now - w->loginat < SECSPERDAY * (DAYSPERWEEK - 1))
				(void)printf("%.3s %-8.5s", p, p + 11);
			else if (now - w->loginat
			      < SECSPERDAY * DAYSPERNYEAR / 2)
				(void)printf("%.6s %-5.5s", p + 4, p + 11);
			else
				(void)printf("%.6s %-5.4s", p + 4, p + 20);
office:
			if (gflag)
				goto no_gecos;
			putchar(' ');
			if (oflag) {
				if (pn->office)
					(void)printf("%-10.10s", pn->office);
				else if (pn->officephone)
					(void)printf("%-10.10s", " ");
				if (pn->officephone)
					(void)printf(" %-.15s",
						    prphone(pn->officephone));
			} else
				(void)printf("%.*s", MAXHOSTNAMELEN, w->host);
no_gecos:
			putchar('\n');
		}
	}
}

static void
stimeprint(WHERE *w)
{
	struct tm *delta;

	delta = gmtime(&w->idletime);
	if (!delta->tm_yday) {
		if (!delta->tm_hour) {
			if (!delta->tm_min)
				(void)printf("    -");
			else
				(void)printf("%5d", delta->tm_min);
		} else
			(void)printf("%2d:%02d",
			    delta->tm_hour, delta->tm_min);
	} else
		(void)printf("%4dd", delta->tm_yday);
}
