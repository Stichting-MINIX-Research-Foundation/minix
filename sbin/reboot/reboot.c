/*	$NetBSD: reboot.c,v 1.40 2012/11/04 22:28:16 christos Exp $	*/

/*
 * Copyright (c) 1980, 1986, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1986, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)reboot.c	8.1 (Berkeley) 6/5/93";
#else
__RCSID("$NetBSD: reboot.c,v 1.40 2012/11/04 22:28:16 christos Exp $");
#endif
#endif /* not lint */

#include <sys/reboot.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif

__dead static void usage(void);

static int dohalt;
static int dopoweroff;

int
main(int argc, char *argv[])
{
	const char *progname;
#if !defined(__minix)
	int i;
#endif /* !defined(__minix) */
	struct passwd *pw;
	int ch, howto, lflag, nflag, qflag, sverrno, len;
	const char *user;
	char *bootstr, **av;

	progname = getprogname();
	if (progname[0] == '-')
		progname++;
	if (strcmp(progname, "halt") == 0) {
		dohalt = 1;
		howto = RB_HALT;
	} else if (strcmp(progname, "poweroff") == 0) {
		dopoweroff = 1;
		howto = RB_HALT | RB_POWERDOWN;
	} else
		howto = 0;
	lflag = nflag = qflag = 0;
	while ((ch = getopt(argc, argv, "dlnpqvxz")) != -1)
		switch(ch) {
		case 'd':
			howto |= RB_DUMP;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			howto |= RB_NOSYNC;
			break;
		case 'p':
			if (dohalt == 0)
				usage();
			howto |= RB_POWERDOWN;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'v':
			howto |= AB_VERBOSE;
			break;
		case 'x':
			howto |= AB_DEBUG;
			break;
		case 'z':
			howto |= AB_SILENT;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc) {
		for (av = argv, len = 0; *av; av++)
			len += strlen(*av) + 1;
		bootstr = malloc(len + 1);
		*bootstr = '\0';		/* for first strcat */
		for (av = argv; *av; av++) {
			strcat(bootstr, *av);
			strcat(bootstr, " ");
		}
		bootstr[len - 1] = '\0';	/* to kill last space */
		howto |= RB_STRING;
	} else
		bootstr = NULL;

	if (geteuid())
		errx(1, "%s", strerror(EPERM));

	if (qflag) {
		reboot(howto, bootstr);
		err(1, "reboot");
	}

	/* Log the reboot. */
	if (!lflag)  {
		if ((user = getlogin()) == NULL)
			user = (pw = getpwuid(getuid())) ?
			    pw->pw_name : "???";
		if (dohalt) {
			openlog("halt", LOG_CONS, LOG_AUTH);
			syslog(LOG_CRIT, "halted by %s", user);
		} else if (dopoweroff) {
			openlog("poweroff", LOG_CONS, LOG_AUTH);
			syslog(LOG_CRIT, "powered off by %s", user);
		} else {
			openlog("reboot", LOG_CONS, LOG_AUTH);
			if (bootstr)
				syslog(LOG_CRIT, "rebooted by %s: %s", user,
				    bootstr);
			else
				syslog(LOG_CRIT, "rebooted by %s", user);
		}
	}
#ifdef SUPPORT_UTMP
	logwtmp("~", "shutdown", "");
#endif
#ifdef SUPPORT_UTMPX
	logwtmpx("~", "shutdown", "", INIT_PROCESS, 0);
#endif

	/*
	 * Do a sync early on, so disks start transfers while we're off
	 * killing processes.  Don't worry about writes done before the
	 * processes die, the reboot system call syncs the disks.
	 */
	if (!nflag)
		sync();

	/* 
	 * Ignore signals that we can get as a result of killing
	 * parents, group leaders, etc.
	 */
	(void)signal(SIGHUP,  SIG_IGN);
	(void)signal(SIGINT,  SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGTSTP, SIG_IGN);

	/*
	 * If we're running in a pipeline, we don't want to die
	 * after killing whatever we're writing to.
	 */
	(void)signal(SIGPIPE, SIG_IGN);

	/* Just stop init -- if we fail, we'll restart it. */
	if (kill(1, SIGTSTP) == -1)
		err(1, "SIGTSTP init");

	/* Send a SIGTERM first, a chance to save the buffers. */
	if (kill(-1, SIGTERM) == -1) {
		/*
		 * If ESRCH, everything's OK: we're the only non-system
		 * process!  That can happen e.g. via 'exec reboot' in
		 * single-user mode.
		 */
		if (errno != ESRCH) {
			warn("SIGTERM all processes");
			goto restart;
		}
	}

	/*
	 * After the processes receive the signal, start the rest of the
	 * buffers on their way.  Wait 5 seconds between the SIGTERM and
	 * the SIGKILL to pretend to give everybody a chance.
	 */
	sleep(2);
	if (!nflag)
		sync();
	sleep(3);

#if !defined(__minix)
	for (i = 1;; ++i) {
		if (kill(-1, SIGKILL) == -1) {
			if (errno == ESRCH)
				break;
			warn("SIGKILL all processes");
			goto restart;
		}
		if (i > 5) {
			warnx("WARNING: some process(es) wouldn't die");
			break;
		}
		(void)sleep(2 * i);
	}
#endif /* !defined(__minix) */

	reboot(howto, bootstr);
	warn("reboot()");
	/* FALLTHROUGH */

restart:
	sverrno = errno;
	errx(1, "%s%s", kill(1, SIGHUP) == -1 ? "(can't restart init): " : "",
	    strerror(sverrno));
	/* NOTREACHED */
}

static void
usage(void)
{
	const char *pflag = dohalt ? "p" : "";

	(void)fprintf(stderr, "usage: %s [-dln%sqvxz] [-- <boot string>]\n",
	    getprogname(), pflag);
	exit(1);
}
