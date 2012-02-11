/*	$NetBSD: siglist.c,v 1.17 2007/01/17 23:24:22 hubertf Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
static char sccsid[] = "@(#)siglist.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: siglist.c,v 1.17 2007/01/17 23:24:22 hubertf Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <signal.h>

static const char *const __siglist14[] = {
	"Signal 0",			/* 0 */
	"Hangup",			/* 1 SIGHUP */
	"Interrupt",			/* 2 SIGINT */
	"Quit",				/* 3 SIGQUIT */
	"Illegal instruction",		/* 4 SIGILL */
	"Trace/BPT trap",		/* 5 SIGTRAP */
	"Abort trap",			/* 6 SIGABRT */
	"EMT trap",			/* 7 SIGEMT */
	"Floating point exception",	/* 8 SIGFPE */
	"Killed",			/* 9 SIGKILL */
	"Bus error",			/* 10 SIGBUS */
	"Segmentation fault",		/* 11 SIGSEGV */
	"Bad system call",		/* 12 SIGSYS */
	"Broken pipe",			/* 13 SIGPIPE */
	"Alarm clock",			/* 14 SIGALRM */
	"Terminated",			/* 15 SIGTERM */
	"Urgent I/O condition",		/* 16 SIGURG */
	"Suspended (signal)",		/* 17 SIGSTOP */
	"Suspended",			/* 18 SIGTSTP */
	"Continued",			/* 19 SIGCONT */
	"Child exited",			/* 20 SIGCHLD */
	"Stopped (tty input)",		/* 21 SIGTTIN */
	"Stopped (tty output)",		/* 22 SIGTTOU */
	"I/O possible",			/* 23 SIGIO */
	"Cputime limit exceeded",	/* 24 SIGXCPU */
	"Filesize limit exceeded",	/* 25 SIGXFSZ */
	"Virtual timer expired",	/* 26 SIGVTALRM */
	"Profiling timer expired",	/* 27 SIGPROF */
	"Window size changes",		/* 28 SIGWINCH */
	"Information request",		/* 29 SIGINFO */
	"User defined signal 1",	/* 30 SIGUSR1 */
	"User defined signal 2",	/* 31 SIGUSR2 */
	"Power fail/restart",		/* 32 SIGPWR */
	"Real time signal 0",		/* 33 SIGRTMIN + 0 */
	"Real time signal 1",		/* 34 SIGRTMIN + 1 */
	"Real time signal 2",		/* 35 SIGRTMIN + 2 */
	"Real time signal 3",		/* 36 SIGRTMIN + 3 */
	"Real time signal 4",		/* 37 SIGRTMIN + 4 */
	"Real time signal 5",		/* 38 SIGRTMIN + 5 */
	"Real time signal 6",		/* 39 SIGRTMIN + 6 */
	"Real time signal 7",		/* 40 SIGRTMIN + 7 */
	"Real time signal 8",		/* 41 SIGRTMIN + 8 */
	"Real time signal 9",		/* 42 SIGRTMIN + 9 */
	"Real time signal 10",		/* 43 SIGRTMIN + 10 */
	"Real time signal 11",		/* 44 SIGRTMIN + 11 */
	"Real time signal 12",		/* 45 SIGRTMIN + 12 */
	"Real time signal 13",		/* 46 SIGRTMIN + 13 */
	"Real time signal 14",		/* 47 SIGRTMIN + 14 */
	"Real time signal 15",		/* 48 SIGRTMIN + 15 */
	"Real time signal 16",		/* 49 SIGRTMIN + 16 */
	"Real time signal 17",		/* 50 SIGRTMIN + 17 */
	"Real time signal 18",		/* 51 SIGRTMIN + 18 */
	"Real time signal 19",		/* 52 SIGRTMIN + 19 */
	"Real time signal 20",		/* 53 SIGRTMIN + 20 */
	"Real time signal 21",		/* 54 SIGRTMIN + 21 */
	"Real time signal 22",		/* 55 SIGRTMIN + 22 */
	"Real time signal 23",		/* 56 SIGRTMIN + 23 */
	"Real time signal 24",		/* 57 SIGRTMIN + 24 */
	"Real time signal 25",		/* 58 SIGRTMIN + 25 */
	"Real time signal 26",		/* 59 SIGRTMIN + 26 */
	"Real time signal 27",		/* 60 SIGRTMIN + 27 */
	"Real time signal 28",		/* 61 SIGRTMIN + 28 */
	"Real time signal 29",		/* 62 SIGRTMIN + 29 */
	"Real time signal 30",		/* 63 SIGRTMIN + 30 */
};

const int __sys_nsig14 = sizeof(__siglist14) / sizeof(__siglist14[0]);

const char * const *__sys_siglist14 = __siglist14;
