/*	$NetBSD: yp_passwd.c,v 1.35 2010/09/08 13:58:46 christos Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993, 1994
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
static char sccsid[] = "from:  @(#)local_passwd.c    8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: yp_passwd.c,v 1.35 2010/09/08 13:58:46 christos Exp $");
#endif
#endif /* not lint */

#ifdef	YP

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <util.h>

#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include "extern.h"

#define passwd yp_passwd_rec
#include <rpcsvc/yppasswd.h>
#undef passwd

#ifndef _PASSWORD_LEN
#define _PASSWORD_LEN PASS_MAX
#endif

static uid_t uid;
static char *domain;

static void
pwerror(const char *name, int show_err, int eval)
{

	if (show_err)
		warn("%s", name);
	errx(eval, "NIS passwd database unchanged");
}

static char *
getnewpasswd(struct passwd *pw, char **old_pass)
{
	int tries;
	const char *p, *t;
	char *result;
	static char buf[_PASSWORD_LEN + 1];
	char salt[_PASSWORD_LEN + 1];
	char option[LINE_MAX], *key, *opt;

	(void)printf("Changing NIS password for %s.\n", pw->pw_name);

	if (old_pass) {
		*old_pass = NULL;
	
		if (pw->pw_passwd[0]) {
			if (strcmp(crypt(p = getpass("Old password:"),
			    pw->pw_passwd),  pw->pw_passwd)) {
				(void)printf("Sorry.\n");
				pwerror(NULL, 0, 1);
			}
		} else {
			p = "";
		}

		*old_pass = strdup(p);
		if (!*old_pass) {
			(void)printf("not enough core.\n");
			pwerror(NULL, 0, 1);
		}
	}
	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (!*p) {
			(void)printf("Password unchanged.\n");
			pwerror(NULL, 0, 0);
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

	pw_getpwconf(option, sizeof(option), pw, "ypcipher");
	opt = option;
	key = strsep(&opt, ",");
	if (pw_gensalt(salt, _PASSWORD_LEN, key, opt) == -1) {
		warn("Couldn't generate salt");
		pwerror(NULL, 0, 0);
	}
	result = strdup(crypt(buf, salt));
	if (!result) {
		(void)printf("not enough core.\n");
		pwerror(NULL, 0, 0);
	}
	return result;
}

static void
makeypp(struct yppasswd *ypp, struct passwd *pw)
{
	/* prompt for new password */
	ypp->newpw.pw_passwd = getnewpasswd(pw, &ypp->oldpass);

	/* tell rpc.yppasswdd */
	ypp->newpw.pw_name	= estrdup(pw->pw_name);
	ypp->newpw.pw_uid 	= pw->pw_uid;
	ypp->newpw.pw_gid	= pw->pw_gid;
	ypp->newpw.pw_gecos	= estrdup(pw->pw_gecos);
	ypp->newpw.pw_dir	= estrdup(pw->pw_dir);
	ypp->newpw.pw_shell	= estrdup(pw->pw_shell);
}

static int
ypgetpwnam(const char *nam, struct passwd *pwd)
{
	char *val;
	int reason, vallen, namlen = (int)strlen(nam);
	int flags;
	int ok;
	
	flags = ok = 0;
	val = NULL;
	reason = yp_match(domain, "master.passwd.byname", nam, namlen,
	    &val, &vallen);
	if (reason == YPERR_MAP) {
		reason = yp_match(domain, "passwd.byname", nam, namlen,
		    &val, &vallen);
		flags = _PASSWORD_OLDFMT;
	}
	if (reason != 0)
		goto out;

	if (pw_scan(val, pwd, &flags) == 0)
		goto out;

	ok = 1;
	val = NULL;	/* Don't free the memory, it is still in use */
out:
	if (val)
		free(val);
	return ok;
}

#ifdef USE_PAM

void
pwyp_usage(const char *prefix)
{

	(void)fprintf(stderr, "%s %s [-d nis | -y] [user]\n",
	    prefix, getprogname());
}

void
pwyp_argv0_usage(const char *prefix)
{

	(void)fprintf(stderr, "%s %s [user]\n",
	    prefix, getprogname());
}

void
pwyp_process(const char *username, int argc, char **argv)
{
	char *master;
	int ch, r, rpcport, status;
	enum clnt_stat yr;
	struct yppasswd ypp;
	struct passwd pwb, pwb2, *pw;
	char pwbuf[1024];
	struct timeval tv;
	CLIENT *client;

	while ((ch = getopt(argc, argv, "y")) != -1) {
		switch (ch) {
		case 'y':
			/*
			 * Abosrb the -y that may have gotten us here.
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
		/*NOTREACHED*/
	}

	if (_yp_check(NULL) == 0) {
		/* can't use YP. */
		errx(EXIT_FAILURE, "NIS not in use.");
	}

	uid = getuid();

	/*
	 * Get local domain
	 */
	if ((r = yp_get_default_domain(&domain)) != 0)
		errx(EXIT_FAILURE, "Can't get local NIS domain (%s)",
		    yperr_string(r));

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	if ((r = yp_master(domain, "passwd.byname", &master)) != 0)
		errx(EXIT_FAILURE, "Can't find the master NIS server (%s)",
		    yperr_string(r));

	/*
	 * Ask the portmapper for the port of the daemon.
	 */
	if ((rpcport = getrpcport(master, YPPASSWDPROG,
	    YPPASSWDPROC_UPDATE, IPPROTO_UDP)) == 0)
		errx(EXIT_FAILURE, "Master NIS server not running yppasswd "
		    "daemon");

	/*
	 * Be sure the port is privileged
	 */
	if (rpcport >= IPPORT_RESERVED)
		errx(EXIT_FAILURE, "Yppasswd daemon is on an invalid port");

	/* Bail out if this is a local (non-yp) user, */
	/* then get user's login identity */
	if (!ypgetpwnam(username, &pwb) ||
	    getpwnam_r(username, &pwb2, pwbuf, sizeof(pwbuf), &pw) ||
	    pw == NULL)
		errx(EXIT_FAILURE, "NIS unknown user %s", username);

	if (uid && uid != pwb.pw_uid) {
		errno = EACCES;
		err(EXIT_FAILURE, "You may only change your own password");
	}

	makeypp(&ypp, &pwb);

	client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (client == NULL)
		errx(EXIT_FAILURE, "Cannot contact yppasswdd on %s (%s)",
		    master, yperr_string(YPERR_YPBIND));

	client->cl_auth = authunix_create_default();
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	yr = clnt_call(client, YPPASSWDPROC_UPDATE,
	    xdr_yppasswd, &ypp, xdr_int, &status, tv);
	if (yr != RPC_SUCCESS)
		errx(EXIT_FAILURE, "RPC to yppasswdd failed (%s)",
		    clnt_sperrno(yr));
	else if (status)
		printf("Couldn't change NIS password.\n");
	else
		printf("The NIS password has been changed on %s, %s\n",
		    master, "the master NIS passwd server.");
}

#else /* ! USE_PAM */

static	int yflag;

int
yp_init(progname)
	const char *progname;
{
	int yppwd;

	if (strcmp(progname, "yppasswd") == 0) {
		yppwd = 1;
	} else
		yppwd = 0;
	yflag = 0;
	if (_yp_check(NULL) == 0) {
		/* can't use YP. */
		if (yppwd)
			errx(EXIT_FAILURE, "NIS not in use");
		return -1;
	}
	return 0;
}

int
yp_arg(char ch, const char *arg)
{
	switch (ch) {
	case 'y':
		yflag = 1;
		break;
	default:
		return 0;
	}
	return 1;
}

int
yp_arg_end(void)
{
	if (yflag)
		return PW_USE_FORCE;
	return PW_USE;
}

void
yp_end(void)
{
	/* NOOP */
}

int
yp_chpw(const char *username)
{
	char *master;
	int r, rpcport, status;
	enum clnt_stat yr;
	struct yppasswd ypp;
	struct passwd *pw, pwb;
	char pwbuf[1024];
	struct timeval tv;
	CLIENT *client;

	uid = getuid();

	/*
	 * Get local domain
	 */
	if ((r = yp_get_default_domain(&domain)) != 0)
		errx(EXIT_FAILURE, "can't get local NIS domain.  Reason: %s",
		    yperr_string(r));

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	if ((r = yp_master(domain, "passwd.byname", &master)) != 0) {
		warnx("can't find the master NIS server.  Reason: %s",
		    yperr_string(r));
		/* continuation */
		return -1;
	}

	/*
	 * Ask the portmapper for the port of the daemon.
	 */
	if ((rpcport = getrpcport(master, YPPASSWDPROG,
	    YPPASSWDPROC_UPDATE, IPPROTO_UDP)) == 0) {
		warnx("Master NIS server not running yppasswd daemon");
		/* continuation */
		return -1;
	}

	/*
	 * Be sure the port is privileged
	 */
	if (rpcport >= IPPORT_RESERVED)
		errx(EXIT_FAILURE, "Yppasswd daemon is on an invalid port");

	/* Bail out if this is a local (non-yp) user, */
	/* then get user's login identity */
	if (!ypgetpwnam(username, pw = &pwb) ||
	    getpwnam_r(username, &pwb, pwbuf, sizeof(pwbuf), &pw) ||
	    pw == NULL) {
		warnx("NIS unknown user %s", username);
		/* continuation */
		return -1;
	}

	if (uid && uid != pw->pw_uid) {
		errno = EACCES;
		err(EXIT_FAILURE, "You may only change your own password");
	}

	makeypp(&ypp, pw);

	client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (client == NULL) {
		warnx("Cannot contact yppasswdd on %s (%s)",
		    master, yperr_string(YPERR_YPBIND));
		return YPERR_YPBIND;
	}

	client->cl_auth = authunix_create_default();
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	yr = clnt_call(client, YPPASSWDPROC_UPDATE,
	    xdr_yppasswd, &ypp, xdr_int, &status, tv);
	if (yr != RPC_SUCCESS)
		errx(EXIT_FAILURE, "RPC to yppasswdd failed (%s)",
		    clnt_sperrno(yr));
	else if (status)
		printf("Couldn't change NIS password.\n");
	else
		printf("The NIS password has been changed on %s, %s\n",
		    master, "the master NIS passwd server.");
	return 0;
}

#endif /* USE_PAM */

#endif	/* YP */
