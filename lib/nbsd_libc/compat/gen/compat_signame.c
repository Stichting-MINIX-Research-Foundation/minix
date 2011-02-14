/*	$NetBSD: compat_signame.c,v 1.1 2005/09/13 01:44:09 christos Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "from: @(#)siglist.c	5.6 (Berkeley) 2/23/91";*/
#else
__RCSID("$NetBSD: compat_signame.c,v 1.1 2005/09/13 01:44:09 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

__warn_references(sys_signame,
    "warning: reference to compatibility sys_signame[]; include <signal.h> for correct reference")

const char *const sys_signame[] = {
	"Signal 0",	/* 0 */
	"HUP",		/* 1 SIGHUP */
	"INT",		/* 2 SIGINT */
	"QUIT",		/* 3 SIGQUIT */
	"ILL",		/* 4 SIGILL */
	"TRAP",		/* 5 SIGTRAP */
	"ABRT",		/* 6 SIGABRT */
	"EMT",		/* 7 SIGEMT */
	"FPE",		/* 8 SIGFPE */
	"KILL",		/* 9 SIGKILL */
	"BUS",		/* 10 SIGBUS */
	"SEGV",		/* 11 SIGSEGV */
	"SYS",		/* 12 SIGSYS */
	"PIPE",		/* 13 SIGPIPE */
	"ALRM",		/* 14 SIGALRM */
	"TERM",		/* 15 SIGTERM */
	"URG",		/* 16 SIGURG */
	"STOP",		/* 17 SIGSTOP */
	"TSTP",		/* 18 SIGTSTP */
	"CONT",		/* 19 SIGCONT */
	"CHLD",		/* 20 SIGCHLD */
	"TTIN",		/* 21 SIGTTIN */
	"TTOU",		/* 22 SIGTTOU */
	"IO",		/* 23 SIGIO */
	"XCPU",		/* 24 SIGXCPU */
	"XFSZ",		/* 25 SIGXFSZ */
	"VTALRM",	/* 26 SIGVTALRM */
	"PROF",		/* 27 SIGPROF */
	"WINCH",	/* 28 SIGWINCH */
	"INFO",		/* 29 SIGINFO */
	"USR1",		/* 30 SIGUSR1 */
	"USR2",		/* 31 SIGUSR2 */
};
