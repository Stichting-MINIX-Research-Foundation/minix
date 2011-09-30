/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>

#ifndef lint
__RCSID("$NetBSD: newgrp.c,v 1.6 2008/04/28 20:24:14 martin Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>

#include <err.h>
#include <grp.h>
#include <libgen.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef LOGIN_CAP
#include <login_cap.h>
#endif

#include "grutil.h"

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-l] [group]\n", getprogname());
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	extern char **environ;
	struct passwd *pwd;
	int c, lflag;
	char *shell, sbuf[MAXPATHLEN + 2];
	uid_t uid;
#ifdef LOGIN_CAP
	login_cap_t *lc;
	u_int flags = LOGIN_SETUSER;
#endif

	uid = getuid();
	pwd = getpwuid(uid);
	if (pwd == NULL)
		errx(EXIT_FAILURE, "who are you?");

#ifdef LOGIN_CAP
	if ((lc = login_getclass(pwd->pw_class)) == NULL)
		errx(EXIT_FAILURE, "%s: unknown login class", pwd->pw_class);
#endif

	(void)setprogname(argv[0]);
	lflag = 0;
	while ((c = getopt(argc, argv, "-l")) != -1) {
		switch (c) {
		case '-':
		case 'l':
			if (lflag)
				usage();
			lflag = 1;
			break;
		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0) {
#if 0
		pwd->pw_gid = newgrp(*argv, pwd);
		addgrp(pwd->pw_gid);
		if (setgid(pwd->pw_gid) < 0)
			err(1, "setgid");
#endif
#ifdef LOGIN_CAP
		addgroup(lc, *argv, pwd, getuid(), "Password:");
#else
		addgroup(*argv, pwd, getuid(), "Password:");
#endif
	} else {
#ifdef LOGIN_CAP
		flags |= LOGIN_SETGROUP;
#else
		if (initgroups(pwd->pw_name, pwd->pw_gid) == -1)
			err(EXIT_FAILURE, "initgroups");
		if (setgid(pwd->pw_gid) == -1)
			err(EXIT_FAILURE, "setgid");
#endif
	}

#ifdef LOGIN_CAP
	if (setusercontext(lc, pwd, uid, flags) == -1)
		err(EXIT_FAILURE, "setusercontext");
	if (!lflag)
		login_close(lc);
#else
	if (setuid(pwd->pw_uid) == -1)
		err(EXIT_FAILURE, "setuid");
#endif

	if (*pwd->pw_shell == '\0') {
#ifdef TRUST_ENV_SHELL
		shell = getenv("SHELL");
		if (shell != NULL)
			pwd->pw_shell = shell;
		else
#endif
			pwd->pw_shell = __UNCONST(_PATH_BSHELL);
	}

	shell = pwd->pw_shell;

	if (lflag) {
		char *term;
#ifdef KERBEROS
		char *krbtkfile;
#endif

		if (chdir(pwd->pw_dir) == -1)
			warn("%s", pwd->pw_dir);

		term = getenv("TERM");
#ifdef KERBEROS
		krbtkfile = getenv("KRBTKFILE");
#endif

		/* create an empty environment */
		if ((environ = malloc(sizeof(char *))) == NULL)
			err(EXIT_FAILURE, NULL);
		environ[0] = NULL;
#ifdef LOGIN_CAP
		if (setusercontext(lc, pwd, uid, LOGIN_SETENV | LOGIN_SETPATH) == -1)
			err(EXIT_FAILURE, "setusercontext");
		login_close(lc);
#else
		(void)setenv("PATH", _PATH_DEFPATH, 1);
#endif
		if (term != NULL)
			(void)setenv("TERM", term, 1);
#ifdef KERBEROS
		if (krbtkfile != NULL)
			(void)setenv("KRBTKFILE", krbtkfile, 1);
#endif

		(void)setenv("LOGNAME", pwd->pw_name, 1);
		(void)setenv("USER", pwd->pw_name, 1);
		(void)setenv("HOME", pwd->pw_dir, 1);
		(void)setenv("SHELL", pwd->pw_shell, 1);

		sbuf[0] = '-';
		(void)strlcpy(sbuf + 1, basename(pwd->pw_shell),
			      sizeof(sbuf) - 1);
		shell = sbuf;
	}

	(void)execl(pwd->pw_shell, shell, NULL);
	err(EXIT_FAILURE, "%s", pwd->pw_shell);
	/* NOTREACHED */
}
