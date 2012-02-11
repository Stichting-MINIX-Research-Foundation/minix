/*	$NetBSD: signame.c,v 1.13 2005/09/13 01:44:09 christos Exp $	*/

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
__RCSID("$NetBSD: signame.c,v 1.13 2005/09/13 01:44:09 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <signal.h>
#include <unistd.h>

static const char *const __signame14[] = {
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
	"PWR",		/* 32 SIGPWR */
	"RT0",		/* 33 SIGRTMIN + 0 */
	"RT1",		/* 34 SIGRTMIN + 1 */
	"RT2",		/* 35 SIGRTMIN + 2 */
	"RT3",		/* 36 SIGRTMIN + 3 */
	"RT4",		/* 37 SIGRTMIN + 4 */
	"RT5",		/* 38 SIGRTMIN + 5 */
	"RT6",		/* 39 SIGRTMIN + 6 */
	"RT7",		/* 40 SIGRTMIN + 7 */
	"RT8",		/* 41 SIGRTMIN + 8 */
	"RT9",		/* 42 SIGRTMIN + 9 */
	"RT10",		/* 43 SIGRTMIN + 10 */
	"RT11",		/* 44 SIGRTMIN + 11 */
	"RT12",		/* 45 SIGRTMIN + 12 */
	"RT13",		/* 46 SIGRTMIN + 13 */
	"RT14",		/* 47 SIGRTMIN + 14 */
	"RT15",		/* 48 SIGRTMIN + 15 */
	"RT16",		/* 49 SIGRTMIN + 16 */
	"RT17",		/* 50 SIGRTMIN + 17 */
	"RT18",		/* 51 SIGRTMIN + 18 */
	"RT19",		/* 52 SIGRTMIN + 19 */
	"RT20",		/* 53 SIGRTMIN + 20 */
	"RT21",		/* 54 SIGRTMIN + 21 */
	"RT22",		/* 55 SIGRTMIN + 22 */
	"RT23",		/* 56 SIGRTMIN + 23 */
	"RT24",		/* 57 SIGRTMIN + 24 */
	"RT25",		/* 58 SIGRTMIN + 25 */
	"RT26",		/* 59 SIGRTMIN + 26 */
	"RT27",		/* 60 SIGRTMIN + 27 */
	"RT28",		/* 61 SIGRTMIN + 28 */
	"RT29",		/* 62 SIGRTMIN + 29 */
	"RT30",		/* 63 SIGRTMIN + 30 */
};

const char * const *__sys_signame14 = __signame14;
