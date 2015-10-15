/*	$NetBSD: trap.c,v 1.37 2015/08/22 12:12:47 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
static char sccsid[] = "@(#)trap.c	8.5 (Berkeley) 6/5/95";
#else
__RCSID("$NetBSD: trap.c,v 1.37 2015/08/22 12:12:47 christos Exp $");
#endif
#endif /* not lint */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "shell.h"
#include "main.h"
#include "nodes.h"	/* for other headers */
#include "eval.h"
#include "jobs.h"
#include "show.h"
#include "options.h"
#include "builtins.h"
#include "syntax.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "trap.h"
#include "mystring.h"
#include "var.h"


/*
 * Sigmode records the current value of the signal handlers for the various
 * modes.  A value of zero means that the current handler is not known.
 * S_HARD_IGN indicates that the signal was ignored on entry to the shell,
 */

#define S_DFL 1			/* default signal handling (SIG_DFL) */
#define S_CATCH 2		/* signal is caught */
#define S_IGN 3			/* signal is ignored (SIG_IGN) */
#define S_HARD_IGN 4		/* signal is ignored permenantly */
#define S_RESET 5		/* temporary - to reset a hard ignored sig */


char *trap[NSIG+1];		/* trap handler commands */
MKINIT char sigmode[NSIG];	/* current value of signal */
static volatile char gotsig[NSIG];/* indicates specified signal received */
volatile int pendingsigs;	/* indicates some signal received */

static int getsigaction(int, sig_t *);

/*
 * return the signal number described by `p' (as a number or a name)
 * or -1 if it isn't one
 */

static int
signame_to_signum(const char *p)
{
	int i;

	if (is_number(p))
		return number(p);

	if (strcasecmp(p, "exit") == 0 )
		return 0;
	
	if (strncasecmp(p, "sig", 3) == 0)
		p += 3;

	for (i = 0; i < NSIG; ++i)
		if (strcasecmp (p, sys_signame[i]) == 0)
			return i;
	return -1;
}

/*
 * Print a list of valid signal names
 */
static void
printsignals(void)
{
	int n;

	out1str("EXIT ");

	for (n = 1; n < NSIG; n++) {
		out1fmt("%s", sys_signame[n]);
		if ((n == NSIG/2) ||  n == (NSIG - 1))
			out1str("\n");
		else
			out1c(' ');
	}
}

/*
 * The trap builtin.
 */

int
trapcmd(int argc, char **argv)
{
	char *action;
	char **ap;
	int signo;

	if (argc <= 1) {
		for (signo = 0 ; signo <= NSIG ; signo++)
			if (trap[signo] != NULL) {
				out1fmt("trap -- ");
				print_quoted(trap[signo]);
				out1fmt(" %s\n",
				    (signo) ? sys_signame[signo] : "EXIT");
			}
		return 0;
	}
	ap = argv + 1;

	action = NULL;

	if (strcmp(*ap, "--") == 0)
		if (*++ap == NULL)
			return 0;

	if (signame_to_signum(*ap) == -1) {
		if ((*ap)[0] == '-') {
			if ((*ap)[1] == '\0')
				ap++;
			else if ((*ap)[1] == 'l' && (*ap)[2] == '\0') {
				printsignals();
				return 0;
			}
			else
				error("bad option %s\n", *ap);
		}
		else
			action = *ap++;
	}

	while (*ap) {
		if (is_number(*ap))
			signo = number(*ap);
		else
			signo = signame_to_signum(*ap);

		if (signo < 0 || signo > NSIG)
			error("%s: bad trap", *ap);

		INTOFF;
		if (action)
			action = savestr(action);

		if (trap[signo])
			ckfree(trap[signo]);

		trap[signo] = action;

		if (signo != 0)
			setsignal(signo, 0);
		INTON;
		ap++;
	}
	return 0;
}



/*
 * Clear traps on a fork or vfork.
 * Takes one arg vfork, to tell it to not be destructive of
 * the parents variables.
 */

void
clear_traps(int vforked)
{
	char **tp;

	for (tp = trap ; tp <= &trap[NSIG] ; tp++) {
		if (*tp && **tp) {	/* trap not NULL or SIG_IGN */
			INTOFF;
			if (!vforked) {
				ckfree(*tp);
				*tp = NULL;
			}
			if (tp != &trap[0])
				setsignal(tp - trap, vforked);
			INTON;
		}
	}
}



/*
 * Set the signal handler for the specified signal.  The routine figures
 * out what it should be set to.
 */

sig_t
setsignal(int signo, int vforked)
{
	int action;
	sig_t sigact = SIG_DFL, sig;
	char *t, tsig;

	if ((t = trap[signo]) == NULL)
		action = S_DFL;
	else if (*t != '\0')
		action = S_CATCH;
	else
		action = S_IGN;
	if (rootshell && !vforked && action == S_DFL) {
		switch (signo) {
		case SIGINT:
			if (iflag || minusc || sflag == 0)
				action = S_CATCH;
			break;
		case SIGQUIT:
#ifdef DEBUG
			if (debug)
				break;
#endif
			/* FALLTHROUGH */
		case SIGTERM:
			if (iflag)
				action = S_IGN;
			break;
#if JOBS
		case SIGTSTP:
		case SIGTTOU:
			if (mflag)
				action = S_IGN;
			break;
#endif
		}
	}

	t = &sigmode[signo - 1];
	tsig = *t;
	if (tsig == 0) {
		/*
		 * current setting unknown
		 */
		if (!getsigaction(signo, &sigact)) {
			/*
			 * Pretend it worked; maybe we should give a warning
			 * here, but other shells don't. We don't alter
			 * sigmode, so that we retry every time.
			 */
			return 0;
		}
		if (sigact == SIG_IGN) {
			/*
			 * POSIX 3.14.13 states that non-interactive shells
			 * should ignore trap commands for signals that were
			 * ignored upon entry, and leaves the behavior
			 * unspecified for interactive shells. On interactive
			 * shells, or if job control is on, and we have a job
			 * control related signal, we allow the trap to work.
			 *
			 * This change allows us to be POSIX compliant, and
			 * at the same time override the default behavior if
			 * we need to by setting the interactive flag.
			 */
			if ((mflag && (signo == SIGTSTP ||
			     signo == SIGTTIN || signo == SIGTTOU)) || iflag) {
				tsig = S_IGN;
			} else
				tsig = S_HARD_IGN;
		} else {
			tsig = S_RESET;	/* force to be set */
		}
	}
	if (tsig == S_HARD_IGN || tsig == action)
		return 0;
	switch (action) {
		case S_DFL:	sigact = SIG_DFL;	break;
		case S_CATCH:  	sigact = onsig;		break;
		case S_IGN:	sigact = SIG_IGN;	break;
	}
	sig = signal(signo, sigact);
	if (sig != SIG_ERR) {
		sigset_t ss;
		if (!vforked)
			*t = action;
		if (action == S_CATCH)
			(void)siginterrupt(signo, 1);
		/*
		 * If our parent accidentally blocked signals for
		 * us make sure we unblock them
		 */
		(void)sigemptyset(&ss);
		(void)sigaddset(&ss, signo);
		(void)sigprocmask(SIG_UNBLOCK, &ss, NULL);
	}
	return sig;
}

/*
 * Return the current setting for sig w/o changing it.
 */
static int
getsigaction(int signo, sig_t *sigact)
{
	struct sigaction sa;

	if (sigaction(signo, (struct sigaction *)0, &sa) == -1)
		return 0;
	*sigact = (sig_t) sa.sa_handler;
	return 1;
}

/*
 * Ignore a signal.
 */

void
ignoresig(int signo, int vforked)
{
	if (sigmode[signo - 1] != S_IGN && sigmode[signo - 1] != S_HARD_IGN) {
		signal(signo, SIG_IGN);
	}
	if (!vforked)
		sigmode[signo - 1] = S_HARD_IGN;
}


#ifdef mkinit
INCLUDE <signal.h>
INCLUDE "trap.h"

SHELLPROC {
	char *sm;

	clear_traps(0);
	for (sm = sigmode ; sm < sigmode + NSIG ; sm++) {
		if (*sm == S_IGN)
			*sm = S_HARD_IGN;
	}
}
#endif



/*
 * Signal handler.
 */

void
onsig(int signo)
{
	signal(signo, onsig);
	if (signo == SIGINT && trap[SIGINT] == NULL) {
		onint();
		return;
	}
	gotsig[signo - 1] = 1;
	pendingsigs++;
}



/*
 * Called to execute a trap.  Perhaps we should avoid entering new trap
 * handlers while we are executing a trap handler.
 */

void
dotrap(void)
{
	int i;
	int savestatus;

	for (;;) {
		for (i = 1 ; ; i++) {
			if (gotsig[i - 1])
				break;
			if (i >= NSIG)
				goto done;
		}
		gotsig[i - 1] = 0;
		savestatus=exitstatus;
		evalstring(trap[i], 0);
		exitstatus=savestatus;
	}
done:
	pendingsigs = 0;
}

int
lastsig(void)
{
	int i;

	for (i = NSIG; i > 0; i--)
		if (gotsig[i - 1])
			return i;
	return SIGINT;	/* XXX */
}

/*
 * Controls whether the shell is interactive or not.
 */


void
setinteractive(int on)
{
	static int is_interactive;

	if (on == is_interactive)
		return;
	setsignal(SIGINT, 0);
	setsignal(SIGQUIT, 0);
	setsignal(SIGTERM, 0);
	is_interactive = on;
}



/*
 * Called to exit the shell.
 */

void
exitshell(int status)
{
	struct jmploc loc1, loc2;
	char *p;

	TRACE(("pid %d, exitshell(%d)\n", getpid(), status));
	if (setjmp(loc1.loc)) {
		goto l1;
	}
	if (setjmp(loc2.loc)) {
		goto l2;
	}
	handler = &loc1;
	if ((p = trap[0]) != NULL && *p != '\0') {
		trap[0] = NULL;
		evalstring(p, 0);
	}
l1:   handler = &loc2;			/* probably unnecessary */
	flushall();
#if JOBS
	setjobctl(0);
#endif
l2:   _exit(status);
	/* NOTREACHED */
}
