/*	$NetBSD: ttyaction.c,v 1.19 2008/04/28 20:23:03 martin Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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

/*
 * For each matching "tty" and "action" run the "command."
 * See fnmatch() for matching the tty name.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ttyaction.c,v 1.19 2008/04/28 20:23:03 martin Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

#ifndef _PATH_TTYACTION
#define _PATH_TTYACTION "/etc/ttyaction"
#endif

static const char *actfile = _PATH_TTYACTION;
static const char *pathenv = "PATH=" _PATH_STDPATH;

int
ttyaction(const char *tty, const char *act, const char *user)
{
	FILE *fp;
	char *p1, *p2;
	const char *argv[4];
	const char *envp[8];
	char *lastp;
	char line[1024];
	char env_tty[64];
	char env_act[64];
	char env_user[256];
	int error, linenum, status;
	pid_t pid;

	_DIAGASSERT(tty != NULL);
	_DIAGASSERT(act != NULL);
	_DIAGASSERT(user != NULL);

	fp = fopen(actfile, "r");
	if (fp == NULL)
		return 0;

	/* Skip the "/dev/" part of the first arg. */
	if (!strncmp(tty, "/dev/", (size_t)5))
		tty += 5;

	/* Args will be: "sh -c ..." */
	argv[0] = _PATH_BSHELL;
	argv[1] = "-c";
	argv[2] = NULL;	/* see below */
	argv[3] = NULL;

	/*
	 * Environment needs: TTY, ACT, USER
	 */
	snprintf(env_tty, sizeof(env_tty), "TTY=%s", tty);
	snprintf(env_act, sizeof(env_act), "ACT=%s", act);
	snprintf(env_user, sizeof(env_user), "USER=%s", user);
	envp[0] = pathenv;
	envp[1] = env_tty;
	envp[2] = env_act;
	envp[3] = env_user;
	envp[4] = NULL;

	linenum = 0;
	status = 0;
	while (fgets(line, (int)sizeof(line), fp)) {
		linenum++;

		/* Allow comment lines. */
		if (line[0] == '#')
			continue;

		p1 = strtok_r(line, " \t", &lastp);
		p2 = strtok_r(NULL, " \t", &lastp);
		/* This arg goes to end of line. */
		argv[2] = strtok_r(NULL, "\n", &lastp);
		if (!p1 || !p2 || !argv[2]) {
			warnx("%s: line %d format error", actfile, linenum);
			continue;
		}
		if (fnmatch(p1, tty, 0) || fnmatch(p2, act, 0))
			continue;
		/* OK, this is a match.  Run the command. */
		pid = fork();
		if (pid == -1) {
			warnx("fork failed: %s", strerror(errno));
			continue;
		}
		if (pid == 0) {
			/* This is the child. */
			error = execve(argv[0], 
			    (char *const *)__UNCONST(argv),
			    (char *const *)__UNCONST(envp));
			/* If we get here, it is an error. */
			warnx("%s: line %d: exec failed: %s",
				  actfile, linenum, strerror(errno));
			_exit(1);
		}
		/* This is the parent. */
		error = waitpid(pid, &status, 0);
		if (error == -1) {
			warnx("%s: line %d: wait failed: %s",
				  actfile, linenum, strerror(errno));
			continue;
		}
		if (WTERMSIG(status)) {
			warnx("%s: line %d: child died with signal %d",
				  actfile, linenum, WTERMSIG(status));
			continue;
		}
	}
	fclose(fp);
	return status;
}
