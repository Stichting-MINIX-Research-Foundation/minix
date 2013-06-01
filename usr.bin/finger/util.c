/*	$NetBSD: util.c,v 1.28 2009/04/12 06:18:54 lukem Exp $	*/

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
 * Portions Copyright (c) 1983, 1995, 1996 Eric P. Allman
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)util.c	8.3 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: util.c,v 1.28 2009/04/12 06:18:54 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <db.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

#include "utmpentry.h"

#include "finger.h"
#include "extern.h"

static void	 find_idle_and_ttywrite(WHERE *);
static void	 userinfo(PERSON *, struct passwd *);
static WHERE	*walloc(PERSON *);

int
match(struct passwd *pw, char *user)
{
	char *p;
	char *bp, name[1024];

	if (!strcasecmp(pw->pw_name, user))
		return(1);

	(void)strlcpy(bp = tbuf, pw->pw_gecos, sizeof(tbuf));

	/* Ampersands get replaced by the login name. */
	if (!(p = strsep(&bp, ",")))
		return(0);

	expandusername(p, pw->pw_name, name, sizeof(name));
	bp = name;
	while ((p = strsep(&bp, "\t ")))
		if (!strcasecmp(p, user))
			return(1);
	return(0);
}

/* inspired by usr.sbin/sendmail/util.c::buildfname */
void
expandusername(const char *gecos, const char *login, char *buf, int buflen)
{
	const char *p;
	char *bp;

	/* why do we skip asterisks!?!? */
	if (*gecos == '*')
		gecos++;
	bp = buf;

	/* copy gecos, interpolating & to be full name */
	for (p = gecos; *p != '\0'; p++) {
		if (bp >= &buf[buflen - 1]) {
			/* buffer overflow - just use login name */
			snprintf(buf, buflen, "%s", login);
			buf[buflen - 1] = '\0';
			return;
		}
		if (*p == '&') {
			/* interpolate full name */
			snprintf(bp, buflen - (bp - buf), "%s", login);
			*bp = toupper((unsigned char)*bp);
			bp += strlen(bp);
		}
		else
			*bp++ = *p;
	}
	*bp = '\0';
}

void
enter_lastlog(PERSON *pn)
{
	WHERE *w;
	static int opened, fd;
	struct lastlog ll;
	char doit = 0;

	/* some systems may not maintain lastlog, don't report errors. */
	if (!opened) {
		fd = open(_PATH_LASTLOG, O_RDONLY, 0);
		opened = 1;
	}
	if (fd == -1 ||
	    lseek(fd, (off_t)pn->uid * sizeof(ll), SEEK_SET) !=
	    (off_t)pn->uid * (off_t)sizeof(ll) ||
	    read(fd, (char *)&ll, sizeof(ll)) != sizeof(ll)) {
			/* as if never logged in */
			ll.ll_line[0] = ll.ll_host[0] = '\0';
			ll.ll_time = 0;
		}
	if ((w = pn->whead) == NULL)
		doit = 1;
	else if (ll.ll_time != 0) {
		/* if last login is earlier than some current login */
		for (; !doit && w != NULL; w = w->next)
			if (w->info == LOGGEDIN && w->loginat < ll.ll_time)
				doit = 1;
		/*
		 * and if it's not any of the current logins
		 * can't use time comparison because there may be a small
		 * discrepency since login calls time() twice
		 */
		for (w = pn->whead; doit && w != NULL; w = w->next)
			if (w->info == LOGGEDIN &&
			    strncmp(w->tty, ll.ll_line, UT_LINESIZE) == 0)
				doit = 0;
	}
	if (doit) {
		w = walloc(pn);
		w->info = LASTLOG;
		if ((w->tty = malloc(UT_LINESIZE + 1)) == NULL)
			err(1, NULL);
		memcpy(w->tty, ll.ll_line, UT_LINESIZE);
		w->tty[UT_LINESIZE] = '\0';
		if ((w->host = malloc(UT_HOSTSIZE + 1)) == NULL)
			err(1, NULL);
		memcpy(w->host, ll.ll_host, UT_HOSTSIZE);
		w->host[UT_HOSTSIZE] = '\0';
		w->loginat = ll.ll_time;
	}
}

void
enter_where(struct utmpentry *ep, PERSON *pn)
{
	WHERE *w = walloc(pn);

	w->info = LOGGEDIN;
	w->tty = ep->line;
	w->host = ep->host;
	w->loginat = (time_t)ep->tv.tv_sec;
	find_idle_and_ttywrite(w);
}

PERSON *
enter_person(struct passwd *pw)
{
	DBT data, key;
	PERSON *pn;

	if (db == NULL &&
	    (db = dbopen(NULL, O_RDWR, 0, DB_BTREE, NULL)) == NULL)
		err(1, NULL);

	key.data = (char *)pw->pw_name;
	key.size = strlen(pw->pw_name);

	switch ((*db->get)(db, &key, &data, 0)) {
	case 0:
		memmove(&pn, data.data, sizeof pn);
		return (pn);
	default:
	case -1:
		err(1, "db get");
		/* NOTREACHED */
	case 1:
		++entries;
		pn = palloc();
		userinfo(pn, pw);
		pn->whead = NULL;

		data.size = sizeof(PERSON *);
		data.data = &pn;
		if ((*db->put)(db, &key, &data, 0))
			err(1, "db put");
		return (pn);
	}
}

PERSON *
find_person(char *name)
{
	DBT data, key;
	PERSON *p;

	if (!db)
		return(NULL);

	key.data = name;
	key.size = strlen(name);

	if ((*db->get)(db, &key, &data, 0))
		return (NULL);
	memmove(&p, data.data, sizeof p);
	return (p);
}

PERSON *
palloc(void)
{
	PERSON *p;

	if ((p = malloc((u_int) sizeof(PERSON))) == NULL)
		err(1, NULL);
	return(p);
}

static WHERE *
walloc(PERSON *pn)
{
	WHERE *w;

	if ((w = malloc((u_int) sizeof(WHERE))) == NULL)
		err(1, NULL);
	if (pn->whead == NULL)
		pn->whead = pn->wtail = w;
	else {
		pn->wtail->next = w;
		pn->wtail = w;
	}
	w->next = NULL;
	return(w);
}

char *
prphone(char *num)
{
	char *p;
	int len;
	static char pbuf[15];

	/* don't touch anything if the user has their own formatting */
	for (p = num; *p; ++p)
		if (!isdigit((unsigned char)*p))
			return(num);
	len = p - num;
	p = pbuf;
	switch(len) {
	case 11:			/* +0-123-456-7890 */
		*p++ = '+';
		*p++ = *num++;
		*p++ = '-';
		/* FALLTHROUGH */
	case 10:			/* 012-345-6789 */
		*p++ = *num++;
		*p++ = *num++;
		*p++ = *num++;
		*p++ = '-';
		/* FALLTHROUGH */
	case 7:				/* 012-3456 */
		*p++ = *num++;
		*p++ = *num++;
		*p++ = *num++;
		break;
	case 5:				/* x0-1234 */
	case 4:				/* x1234 */
		*p++ = 'x';
		*p++ = *num++;
		break;
	default:
		return(num);
	}
	if (len != 4) {
		*p++ = '-';
		*p++ = *num++;
	}
	*p++ = *num++;
	*p++ = *num++;
	*p++ = *num++;
	*p = '\0';
	return(pbuf);
}

static void
find_idle_and_ttywrite(WHERE *w)
{
	struct stat sb;

	(void)snprintf(tbuf, sizeof(tbuf), "%s/%s", _PATH_DEV, w->tty);
	if (stat(tbuf, &sb) < 0) {
		warn("%s", tbuf);
		return;
	}
	w->idletime = now < sb.st_atime ? 0 : now - sb.st_atime;

#define	TALKABLE	0220		/* tty is writable if 220 mode */
	w->writable = ((sb.st_mode & TALKABLE) == TALKABLE);
}

static void
userinfo(PERSON *pn, struct passwd *pw)
{
	char *p;
	char *bp, name[1024];
	struct stat sb;

	pn->realname = pn->office = pn->officephone = pn->homephone = NULL;

	pn->uid = pw->pw_uid;
	pn->name = strdup(pw->pw_name);
	pn->dir = strdup(pw->pw_dir);
	pn->shell = strdup(pw->pw_shell);

	(void)strlcpy(bp = tbuf, pw->pw_gecos, sizeof(tbuf));

	/* ampersands get replaced by the login name */
	if (!(p = strsep(&bp, ",")))
		return;
	expandusername(p, pw->pw_name, name, sizeof(name));
	pn->realname = strdup(name);
	pn->office = ((p = strsep(&bp, ",")) && *p) ?
	    strdup(p) : NULL;
	pn->officephone = ((p = strsep(&bp, ",")) && *p) ?
	    strdup(p) : NULL;
	pn->homephone = ((p = strsep(&bp, ",")) && *p) ?
	    strdup(p) : NULL;
	(void)snprintf(tbuf, sizeof(tbuf), "%s/%s", _PATH_MAILDIR,
	    pw->pw_name);
	pn->mailrecv = -1;		/* -1 == not_valid */
	if (stat(tbuf, &sb) < 0) {
		if (errno != ENOENT) {
			(void)fprintf(stderr,
			    "finger: %s: %s\n", tbuf, strerror(errno));
			return;
		}
	} else if (sb.st_size != 0) {
		pn->mailrecv = sb.st_mtime;
		pn->mailread = sb.st_atime;
	}
}
