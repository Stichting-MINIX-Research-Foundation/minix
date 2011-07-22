/*	$NetBSD: system.c,v 1.23 2010/11/14 18:11:43 tron Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)system.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: system.c,v 1.23 2010/11/14 18:11:43 tron Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>

#include "env.h"
#include "reentrant.h"

int
system(command)
	const char *command;
{
	pid_t pid;
	struct sigaction intsa, quitsa, sa;
	sigset_t nmask, omask;
	int pstat;
	const char *argp[] = {"sh", "-c", NULL, NULL};
	argp[2] = command;

	/*
	 * ISO/IEC 9899:1999 in 7.20.4.6 describes this special case.
	 * We need to check availability of a command interpreter.
	 */
	if (command == NULL) {
		if (access(_PATH_BSHELL, X_OK) == 0)
			return 1;
		return 0;
	}

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGINT, &sa, &intsa) == -1)
		return -1;
	if (sigaction(SIGQUIT, &sa, &quitsa) == -1) {
		sigaction(SIGINT, &intsa, NULL);
		return -1;
	}

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &nmask, &omask) == -1) {
		sigaction(SIGINT, &intsa, NULL);
		sigaction(SIGQUIT, &quitsa, NULL);
		return -1;
	}

	(void)__readlockenv();
	switch(pid = vfork()) {
	case -1:			/* error */
		(void)__unlockenv();
		sigaction(SIGINT, &intsa, NULL);
		sigaction(SIGQUIT, &quitsa, NULL);
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		return -1;
	case 0:				/* child */
		sigaction(SIGINT, &intsa, NULL);
		sigaction(SIGQUIT, &quitsa, NULL);
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		execve(_PATH_BSHELL, __UNCONST(argp), environ);
		_exit(127);
	}
	(void)__unlockenv();

	while (waitpid(pid, &pstat, 0) == -1) {
		if (errno != EINTR) {
			pstat = -1;
			break;
		}
	}

	sigaction(SIGINT, &intsa, NULL);
	sigaction(SIGQUIT, &quitsa, NULL);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);

	return (pstat);
}
