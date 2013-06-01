/*	$NetBSD: finger.c,v 1.29 2009/04/12 06:18:54 lukem Exp $	*/

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

/*
 * Luke Mewburn <lukem@NetBSD.org> added the following on 961121:
 *    - mail status ("No Mail", "Mail read:...", or "New Mail ...,
 *	Unread since ...".)
 *    - 4 digit phone extensions (3210 is printed as x3210.)
 *    - host/office toggling in short format with -h & -o.
 *    - short day names (`Tue' printed instead of `Jun 21' if the
 *	login time is < 6 days.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)finger.c	8.5 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: finger.c,v 1.29 2009/04/12 06:18:54 lukem Exp $");
#endif
#endif /* not lint */

/*
 * Finger prints out information about users.  It is not portable since
 * certain fields (e.g. the full user name, office, and phone numbers) are
 * extracted from the gecos field of the passwd file which other UNIXes
 * may not have or may use for other things.
 *
 * There are currently two output formats; the short format is one line
 * per user and displays login name, tty, login time, real name, idle time,
 * and either remote host information (default) or office location/phone
 * number, depending on if -h or -o is used respectively.
 * The long format gives the same information (in a more legible format) as
 * well as home directory, shell, mail info, and .plan/.project files.
 */

#include <sys/param.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <locale.h>
#include <langinfo.h>

#include "utmpentry.h"

#include "finger.h"
#include "extern.h"

DB *db;
time_t now;
int entries, gflag, lflag, mflag, oflag, sflag, eightflag, pplan;
char tbuf[1024];
struct utmpentry *ehead;

static void loginlist(void);
static void userlist(int, char **);
int main(int, char **);

int
main(int argc, char **argv)
{
	int ch;

	/* Allow user's locale settings to affect character output. */
	setlocale(LC_CTYPE, "");

	/*
	 * Reset back to the C locale, unless we are using a known
	 * single-byte 8-bit locale.
	 */
	if (strncmp(nl_langinfo(CODESET), "ISO8859-", 8))
		setlocale(LC_CTYPE, "C");

	oflag = 1;		/* default to old "office" behavior */

	while ((ch = getopt(argc, argv, "lmpshog8")) != -1)
		switch(ch) {
		case 'l':
			lflag = 1;		/* long format */
			break;
		case 'm':
			mflag = 1;		/* force exact match of names */
			break;
		case 'p':
			pplan = 1;		/* don't show .plan/.project */
			break;
		case 's':
			sflag = 1;		/* short format */
			break;
		case 'h':
			oflag = 0;		/* remote host info */
			break;
		case 'o':
			oflag = 1;		/* office info */
			break;
		case 'g':
			gflag = 1;		/* no gecos info, besides name */
			break;
		case '8':
			eightflag = 1;		/* 8-bit pass-through */
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: finger [-lmpshog8] [login ...]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	(void)time(&now);
	setpassent(1);
	entries = getutentries(NULL, &ehead);
	if (argc == 0) {
		/*
		 * Assign explicit "small" format if no names given and -l
		 * not selected.  Force the -s BEFORE we get names so proper
		 * screening will be done.
		 */
		if (!lflag)
			sflag = 1;	/* if -l not explicit, force -s */
		loginlist();
		if (entries == 0)
			(void)printf("No one logged on.\n");
	} else {
		userlist(argc, argv);
		/*
		 * Assign explicit "large" format if names given and -s not
		 * explicitly stated.  Force the -l AFTER we get names so any
		 * remote finger attempts specified won't be mishandled.
		 */
		if (!sflag)
			lflag = 1;	/* if -s not explicit, force -l */
	}
	if (entries) {
		if (lflag)
			lflag_print();
		else
			sflag_print();
	}
	return (0);
}

static void
loginlist(void)
{
	PERSON *pn;
	DBT data, key;
	struct passwd *pw;
	int r, seqflag;
	struct utmpentry *ep;

	for (ep = ehead; ep; ep = ep->next) {
		if ((pn = find_person(ep->name)) == NULL) {
			if ((pw = getpwnam(ep->name)) == NULL)
				continue;
			pn = enter_person(pw);
		}
		enter_where(ep, pn);
	}
	if (db && lflag)
		for (seqflag = R_FIRST;; seqflag = R_NEXT) {
			PERSON *tmp;

			r = (*db->seq)(db, &key, &data, seqflag);
			if (r == -1)
				err(1, "db seq");
			if (r == 1)
				break;
			memmove(&tmp, data.data, sizeof tmp);
			enter_lastlog(tmp);
		}
}

static void
userlist(int argc, char **argv)
{
	PERSON *pn;
	DBT data, key;
	struct passwd *pw;
	int r, seqflag, *used, *ip;
	char **ap, **nargv, **np, **p;
	struct utmpentry *ep;

	if ((nargv = malloc((argc+1) * sizeof(char *))) == NULL ||
	    (used = calloc(argc, sizeof(int))) == NULL)
		err(1, NULL);

	/* Pull out all network requests. */
	for (ap = p = argv, np = nargv; *p; ++p)
		if (strchr(*p, '@'))
			*np++ = *p;
		else
			*ap++ = *p;

	*np++ = NULL;
	*ap++ = NULL;

	if (!*argv)
		goto net;

	/*
	 * Traverse the list of possible login names and check the login name
	 * and real name against the name specified by the user.
	 */
	if (mflag) {
		for (p = argv; *p; ++p)
			if ((pw = getpwnam(*p)) != NULL)
				enter_person(pw);
			else
				warnx("%s: no such user", *p);
	} else {
		while ((pw = getpwent()) != NULL)
			for (p = argv, ip = used; *p; ++p, ++ip)
				if (match(pw, *p)) {
					enter_person(pw);
					*ip = 1;
				}
		for (p = argv, ip = used; *p; ++p, ++ip)
			if (!*ip)
				warnx("%s: no such user", *p);
	}

	/* Handle network requests. */
net:
	for (p = nargv; *p;)
		netfinger(*p++);

	if (entries == 0)
		goto done;

	/*
	 * Scan thru the list of users currently logged in, saving
	 * appropriate data whenever a match occurs.
	 */
	for (ep = ehead; ep; ep = ep->next) {
		if ((pn = find_person(ep->name)) == NULL)
			continue;
		enter_where(ep, pn);
	}
	if (db != NULL)
		for (seqflag = R_FIRST;; seqflag = R_NEXT) {
			PERSON *tmp;

			r = (*db->seq)(db, &key, &data, seqflag);
			if (r == -1)
				err(1, "db seq");
			if (r == 1)
				break;
			memmove(&tmp, data.data, sizeof tmp);
			enter_lastlog(tmp);
		}
done:
	free(nargv);
	free(used);
}
