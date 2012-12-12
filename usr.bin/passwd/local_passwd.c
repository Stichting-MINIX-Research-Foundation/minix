/*	$NetBSD: local_passwd.c,v 1.36 2012/03/25 05:55:07 dholland Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 * 	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "from: @(#)local_passwd.c    8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: local_passwd.c,v 1.36 2012/03/25 05:55:07 dholland Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <login_cap.h>
#include <syslog.h>

#include "extern.h"

static uid_t uid;

static char *
getnewpasswd(struct passwd *pw, int min_pw_len)
{
	int tries;
	char *p, *t;
	char buf[_PASSWORD_LEN+1], salt[_PASSWORD_LEN+1];
	char option[LINE_MAX], *key, *opt;
	
	(void)printf("Changing local password for %s.\n", pw->pw_name);

	if (uid && pw->pw_passwd[0] &&
	    strcmp(crypt(getpass("Old password:"), pw->pw_passwd),
	    pw->pw_passwd)) {
		errno = EACCES;
		syslog(LOG_AUTH | LOG_NOTICE,
		       "user %s (UID %lu) failed to change the "
		       "local password of user %s: %m",
		       pw->pw_name, (unsigned long)uid, pw->pw_name);
		pw_error(NULL, 1, 1);
	}

	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (!*p) {
			(void)printf("Password unchanged.\n");
			pw_error(NULL, 0, 0);
		}
		if (min_pw_len > 0 && (int)strlen(p) < min_pw_len) {
			(void) printf("Password is too short.\n");
			continue;
		}
		if (strlen(p) <= 5 && ++tries < 2) {
			(void)printf("Please enter a longer password.\n");
			continue;
		}
		for (t = p; *t && islower((unsigned char)*t); ++t);
		if (!*t && ++tries < 2) {
			(void)printf("Please don't use an all-lower case "
				     "password.\nUnusual capitalization, "
				     "control characters or digits are "
				     "suggested.\n");
			continue;
		}
		(void)strlcpy(buf, p, sizeof(buf));
		if (!strcmp(buf, getpass("Retype new password:")))
			break;
		(void)printf("Mismatch; try again, EOF to quit.\n");
	}

	pw_getpwconf(option, sizeof(option), pw, "localcipher");
	opt = option;
	key = strsep(&opt, ",");
	if(pw_gensalt(salt, _PASSWORD_LEN, key, opt) == -1) {
		warn("Couldn't generate salt");
		pw_error(NULL, 0, 0);
	}
	return(crypt(buf, salt));
}

#ifdef USE_PAM

void
pwlocal_usage(const char *prefix)
{

	(void) fprintf(stderr, "%s %s [-d files | -l] [user]\n",
	    prefix, getprogname());
}

void
pwlocal_process(const char *username, int argc, char **argv)
{
	struct passwd *pw;
	struct passwd old_pw;
	time_t old_change;
	int pfd, tfd;
	int min_pw_len = 0;
	int pw_expiry  = 0;
	int ch;
#ifdef LOGIN_CAP
	login_cap_t *lc;
#endif

	while ((ch = getopt(argc, argv, "l")) != -1) {
		switch (ch) {
		case 'l':
			/*
			 * Aborb the -l that may have gotten us here.
			 */
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	switch (argc) {
	case 0:
		/* username already provided */
		break;
	case 1:
		username = argv[0];
		break;
	default:
		usage();
		/* NOTREACHED */
	}

	if (!(pw = getpwnam(username)))
		errx(1, "unknown user %s", username);

	uid = getuid();
	if (uid && uid != pw->pw_uid)
		errx(1, "%s", strerror(EACCES));

	/* Save the old pw information for comparing on pw_copy(). */
	old_pw = *pw;

	/*
	 * Get class restrictions for this user, then get the new password. 
	 */
#ifdef LOGIN_CAP
	if((lc = login_getclass(pw->pw_class)) != NULL) {
		min_pw_len = (int) login_getcapnum(lc, "minpasswordlen", 0, 0);
		pw_expiry  = (int) login_getcaptime(lc, "passwordtime", 0, 0);
		login_close(lc);
	}
#endif

	pw->pw_passwd = getnewpasswd(pw, min_pw_len);
	old_change = pw->pw_change;
	pw->pw_change = pw_expiry ? pw_expiry + time(NULL) : 0;

	/*
	 * Now that the user has given us a new password, let us
	 * change the database.
	 */
	pw_init();
	tfd = pw_lock(0);
	if (tfd < 0) {
		warnx ("The passwd file is busy, waiting...");
		tfd = pw_lock(10);
		if (tfd < 0)
			errx(1, "The passwd file is still busy, "
			     "try again later.");
	}

	pfd = open(_PATH_MASTERPASSWD, O_RDONLY, 0);
	if (pfd < 0)
		pw_error(_PATH_MASTERPASSWD, 1, 1);

	pw_copy(pfd, tfd, pw, &old_pw);

	if (pw_mkdb(username, old_change == pw->pw_change) < 0)
		pw_error(NULL, 0, 1);

	syslog(LOG_AUTH | LOG_INFO,
	       "user %s (UID %lu) successfully changed "
	       "the local password of user %s",
	       uid ? username : "root", (unsigned long)uid, username);
}

#else /* ! USE_PAM */

static int force_local;

int
local_init(const char *progname)
{
	force_local = 0;
	return (0);
}

int
local_arg(char ch, const char *arg)
{
	switch (ch) {
	case 'l':
		force_local = 1;
		break;
	default:
		return(0);
	}
	return(1);
}

int
local_arg_end(void)
{
	if (force_local)
		return(PW_USE_FORCE);
	return(PW_USE);
}

void
local_end(void)
{
	/* NOOP */
}

int
local_chpw(const char *uname)
{
	struct passwd *pw;
	struct passwd old_pw;
	time_t old_change;
	int pfd, tfd;
	int min_pw_len = 0;
	int pw_expiry  = 0;
#ifdef LOGIN_CAP
	login_cap_t *lc;
#endif
	
	if (!(pw = getpwnam(uname))) {
		warnx("unknown user %s", uname);
		return (1);
	}

	uid = getuid();
	if (uid && uid != pw->pw_uid) {
		warnx("%s", strerror(EACCES));
		return (1);
	}

	/* Save the old pw information for comparing on pw_copy(). */
	old_pw = *pw;

	/*
	 * Get class restrictions for this user, then get the new password. 
	 */
#ifdef LOGIN_CAP
	if((lc = login_getclass(pw->pw_class))) {
		min_pw_len = (int) login_getcapnum(lc, "minpasswordlen", 0, 0);
		pw_expiry  = (int) login_getcaptime(lc, "passwordtime", 0, 0);
		login_close(lc);
	}
#endif

	pw->pw_passwd = getnewpasswd(pw, min_pw_len);
	old_change = pw->pw_change;
	pw->pw_change = pw_expiry ? pw_expiry + time(NULL) : 0;

	/*
	 * Now that the user has given us a new password, let us
	 * change the database.
	 */
	pw_init();
	tfd = pw_lock(0);
	if (tfd < 0) {
		warnx ("The passwd file is busy, waiting...");
		tfd = pw_lock(10);
		if (tfd < 0)
			errx(1, "The passwd file is still busy, "
			     "try again later.");
	}

	pfd = open(_PATH_MASTERPASSWD, O_RDONLY, 0);
	if (pfd < 0)
		pw_error(_PATH_MASTERPASSWD, 1, 1);

	pw_copy(pfd, tfd, pw, &old_pw);

	if (pw_mkdb(uname, old_change == pw->pw_change) < 0)
		pw_error(NULL, 0, 1);

	syslog(LOG_AUTH | LOG_INFO,
	       "user %s (UID %lu) successfully changed "
	       "the local password of user %s",
	       uid ? uname : "root", (unsigned long)uid, uname);

	return (0);
}

#endif /* USE_PAM */
