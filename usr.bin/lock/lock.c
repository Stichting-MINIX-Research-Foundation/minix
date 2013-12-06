/*	$NetBSD: lock.c,v 1.33 2013/10/18 20:47:06 christos Exp $	*/

/*
 * Copyright (c) 1980, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Bob Toxen.
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1987, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)lock.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: lock.c,v 1.33 2013/10/18 20:47:06 christos Exp $");
#endif /* not lint */

/*
 * Lock a terminal up until the given key is entered, until the root
 * password is entered, or the given interval times out.
 *
 * Timeout interval is by default TIMEOUT, it can be changed with
 * an argument of the form -time where time is in minutes
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>

#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/openpam.h>	/* for openpam_ttyconv() */
#endif

#ifdef SKEY
#include <skey.h>
#endif


#define	TIMEOUT	15

int	main(int, char **);

static void	bye(int) __dead;
static void	hi(int);
static void	quit(int) __dead;
#ifdef SKEY
static int	skey_auth(const char *);
#endif

static struct timeval	timeout;
static struct timeval	zerotime;
static struct termios	tty, ntty;
static int	notimeout;			/* no timeout at all */
static long	nexttime;			/* keep the timeout time */

int
main(int argc, char **argv)
{
	struct passwd *pw;
	struct timeval timval;
	struct itimerval ntimer, otimer;
	struct tm *timp;
	time_t curtime;
	int ch, usemine;
	long sectimeout;
	char *ap, *ttynam;
	const char *tzn;
	uid_t uid = getuid();
	char hostname[MAXHOSTNAMELEN + 1], s[BUFSIZ], s1[BUFSIZ];
#ifdef USE_PAM
	pam_handle_t *pamh = NULL;
	static const struct pam_conv pamc = { &openpam_ttyconv, NULL };
	int pam_err;
#else
	char *mypw = NULL;
#endif

	if ((pw = getpwuid(getuid())) == NULL)
		errx(1, "unknown uid %lu.", (u_long)uid);

	notimeout = 0;
	sectimeout = TIMEOUT;
	usemine = 0;

	while ((ch = getopt(argc, argv, "npt:")) != -1)
		switch ((char)ch) {
		case 'n':
			notimeout = 1;
			break;
		case 't':
			errno = 0;
			if (((sectimeout = strtol(optarg, &ap, 0)) == LONG_MAX
			    || sectimeout == LONG_MIN)
			    && errno == ERANGE)
				err(1, "illegal timeout value: %s", optarg);
			if (optarg == ap || *ap || sectimeout <= 0)
				errx(1, "illegal timeout value: %s", optarg);
			if (sectimeout >= INT_MAX / 60)
				errx(1, "too large timeout value: %ld",
				    sectimeout);
			break;
		case 'p':
			usemine = 1;
#ifndef USE_PAM
			mypw = strdup(pw->pw_passwd);
			if (!mypw)
				err(1, "strdup");
#endif
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: %s [-np] [-t timeout]\n", getprogname());
			exit(1);
		}

#if defined(USE_PAM) || defined(SKEY)
	if (! usemine) {	/* -p with PAM or S/key needs privs */
#endif
	if (setuid(uid) == -1)	/* discard privs */
		err(1, "setuid failed");
#if defined(USE_PAM) || defined(SKEY)
	}
#endif

	timeout.tv_sec = (int)sectimeout * 60;

	if (tcgetattr(STDIN_FILENO, &tty) < 0)	/* get information for header */
		err(1, "tcgetattr failed");
	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';
	if (!(ttynam = ttyname(STDIN_FILENO)))
		err(1, "ttyname failed");
	if (gettimeofday(&timval, NULL) == -1)
		err(1, "gettimeofday failed");
	curtime = timval.tv_sec;
	nexttime = timval.tv_sec + ((int)sectimeout * 60);
	timp = localtime(&curtime);
	ap = asctime(timp);
#ifdef __SVR4
	tzn = tzname[0];
#else
	tzn = timp->tm_zone;
#endif

	if (signal(SIGINT, quit) == SIG_ERR)
		err(1, "signal failed");
	if (signal(SIGQUIT, quit) == SIG_ERR)
		err(1, "signal failed");
	ntty = tty; ntty.c_lflag &= ~ECHO;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &ntty) == -1)
		err(1, "tcsetattr");

	if (!usemine) {
		/* get key and check again */
		(void)printf("Key: ");
		if (!fgets(s, sizeof(s), stdin) || *s == '\n')
			quit(0);
		(void)printf("\nAgain: ");
		/*
		 * Don't need EOF test here, if we get EOF, then s1 != s
		 * and the right things will happen.
		 */
		(void)fgets(s1, sizeof(s1), stdin);
		(void)putchar('\n');
		if (strcmp(s1, s)) {
			(void)printf("\alock: passwords didn't match.\n");
			(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &tty);
			exit(1);
		}
		s[0] = '\0';
#ifndef USE_PAM
		mypw = s1;
#endif
	}
#ifdef USE_PAM
	if (usemine) {
		pam_err = pam_start("lock", pw->pw_name, &pamc, &pamh);
		if (pam_err != PAM_SUCCESS)
			err(1, "pam_start: %s", pam_strerror(NULL, pam_err));
	}
#endif

	/* set signal handlers */
	if (signal(SIGINT, hi) == SIG_ERR)
		err(1, "signal failed");
	if (signal(SIGQUIT, hi) == SIG_ERR)
		err(1, "signal failed");
	if (signal(SIGTSTP, hi) == SIG_ERR)
		err(1, "signal failed");

	if (notimeout) {
		if (signal(SIGALRM, hi) == SIG_ERR)
			err(1, "signal failed");
		(void)printf("lock: %s on %s.  no timeout.\n"
		    "time now is %.20s%s%s",
		    ttynam, hostname, ap, tzn, ap + 19);
	}
	else {
		if (signal(SIGALRM, bye) == SIG_ERR)
			err(1, "signal failed");

		ntimer.it_interval = zerotime;
		ntimer.it_value = timeout;
		if (setitimer(ITIMER_REAL, &ntimer, &otimer) == -1)
			err(1, "setitimer failed");

		/* header info */
		(void)printf("lock: %s on %s. timeout in %ld minutes\n"
		    "time now is %.20s%s%s",
		    ttynam, hostname, sectimeout, ap, tzn, ap + 19);
	}

	for (;;) {
#ifdef USE_PAM
		if (usemine) {
			pam_err = pam_authenticate(pamh, 0);
			if (pam_err == PAM_SUCCESS)
				break;
			goto tryagain;
		}
#endif
		(void)printf("Key: ");
		if (!fgets(s, sizeof(s), stdin)) {
			clearerr(stdin);
			hi(0);
			goto tryagain;
		}
#ifndef USE_PAM
		if (usemine) {
			s[strlen(s) - 1] = '\0';
#ifdef SKEY
			if (strcasecmp(s, "s/key") == 0) {
				if (skey_auth(pw->pw_name))
					break;
			}
#endif
			if (!strcmp(mypw, crypt(s, mypw)))
				break;
		}
		else
#endif
			if (!strcmp(s, s1))
				break;
		(void)printf("\a\n");
 tryagain:
		if (tcsetattr(STDIN_FILENO, TCSADRAIN, &ntty) == -1
		    && errno != EINTR)
			err(1, "tcsetattr failed");
	}
#ifdef USE_PAM
	if (usemine) {
		(void)pam_end(pamh, pam_err);
	}
#endif
	quit(0);
	/* NOTREACHED */
	return 0;
}

#ifdef SKEY
/*
 * We can't use libskey's skey_authenticate() since it
 * handles signals in a way that's inappropriate
 * for our needs. Instead we roll our own.
 */
static int
skey_auth(const char *user)
{
	char s[128];
	const char *ask;
	int ret = 0;

	if (!skey_haskey(user) && (ask = skey_keyinfo(user))) {
		(void)printf("\n[%s]\nResponse: ", ask);
		if (!fgets(s, sizeof(s), stdin) || *s == '\n')
			clearerr(stdin);
		else {
			s[strlen(s) - 1] = '\0';
			if (skey_passcheck(user, s) != -1)
				ret = 1;
		}
	} else
		(void)printf("Sorry, you have no s/key.\n");
	return ret;
}
#endif

static void
hi(int dummy)
{
	struct timeval timval;

	if (notimeout)
		(void)printf("lock: type in the unlock key.\n");
	else {
		if (gettimeofday(&timval, NULL) == -1)
			err(1, "gettimeofday failed");
		(void)printf("lock: type in the unlock key. "
		    "timeout in %lld:%lld minutes\n",
		    (long long)(nexttime - timval.tv_sec) / 60,
		    (long long)(nexttime - timval.tv_sec) % 60);
	}
}

static void
quit(int dummy)
{
	(void)putchar('\n');
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &tty);
	exit(0);
}

static void
bye(int dummy)
{
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &tty);
	(void)printf("lock: timeout\n");
	exit(1);
}
