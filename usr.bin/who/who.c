/*	$NetBSD: who.c,v 1.24 2014/06/08 09:53:43 mlelstv Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)who.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: who.c,v 1.24 2014/06/08 09:53:43 mlelstv Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif

#include "utmpentry.h"

static void output_labels(void);
static void who_am_i(const char *, int);
static void usage(void) __dead;
static void process(const char *, int);
static void eprint(const struct utmpentry *);
static void print(const char *, const char *, time_t, const char *, pid_t pid,
    uint16_t term, uint16_t xit, uint16_t sess, uint16_t type);
static void quick(const char *);

static int show_term;			/* show term state */
static int show_idle;			/* show idle time */
static int show_details;		/* show exit status etc. */

struct ut_type_names {
  int type;
  const char *name;
} ut_type_names[] = {
#ifdef SUPPORT_UTMPX
  { EMPTY, "empty" }, 
  { RUN_LVL, "run level" }, 
  { BOOT_TIME, "boot time" }, 
  { OLD_TIME, "old time" }, 
  { NEW_TIME, "new time" }, 
  { INIT_PROCESS, "init process" }, 
  { LOGIN_PROCESS, "login process" }, 
  { USER_PROCESS, "user process" }, 
  { DEAD_PROCESS, "dead process" }, 
#if defined(_NETBSD_SOURCE)
  { ACCOUNTING, "accounting" }, 
  { SIGNATURE, "signature" },
  { DOWN_TIME, "down time" },
#endif /* _NETBSD_SOURCE */
#endif /* SUPPORT_UTMPX */
  { -1, "unknown" }
};

int
main(int argc, char *argv[])
{
	int c, only_current_term, show_labels, quick_mode, default_mode;
	int et = 0;

	setlocale(LC_ALL, "");

	only_current_term = show_term = show_idle = show_labels = 0;
	quick_mode = default_mode = 0;

	while ((c = getopt(argc, argv, "abdHlmpqrsTtuv")) != -1) {
		switch (c) {
		case 'a':
			et = -1;
			show_idle = show_details = 1;
			break;
		case 'b':
			et |= (1 << BOOT_TIME);
			break;
		case 'd':
			et |= (1 << DEAD_PROCESS);
			break;
		case 'H':
			show_labels = 1;
			break;
		case 'l':
			et |= (1 << LOGIN_PROCESS);
			break;
		case 'm':
			only_current_term = 1;
			break;
		case 'p':
			et |= (1 << INIT_PROCESS);
			break;
		case 'q':
			quick_mode = 1;
			break;
		case 'r':
			et |= (1 << RUN_LVL);
			break;
		case 's':
			default_mode = 1;
			break;
		case 'T':
			show_term = 1;
			break;
		case 't':
			et |= (1 << NEW_TIME);
			break;
		case 'u':
			show_idle = 1;
			break;
		case 'v':
			show_details = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (et != 0)
		etype = et;

	if (chdir("/dev")) {
		err(EXIT_FAILURE, "cannot change directory to /dev");
		/* NOTREACHED */
	}

	if (default_mode)
		only_current_term = show_term = show_idle = 0;

	switch (argc) {
	case 0:					/* who */
		if (quick_mode) {
			quick(NULL);
		} else if (only_current_term) {
			who_am_i(NULL, show_labels);
		} else {
			process(NULL, show_labels);
		}
		break;
	case 1:					/* who utmp_file */
		if (quick_mode) {
			quick(*argv);
		} else if (only_current_term) {
			who_am_i(*argv, show_labels);
		} else {
			process(*argv, show_labels);
		}
		break;
	case 2:					/* who am i */
		who_am_i(NULL, show_labels);
		break;
	default:
		usage();
		/* NOTREACHED */
	}

	return 0;
}

static char *
strrstr(const char *str, const char *pat)
{
	const char *estr;
	size_t len;
	if (*pat == '\0')
		return __UNCONST(str);

	len = strlen(pat);

	for (estr = str + strlen(str); str < estr; estr--)
		if (strncmp(estr, pat, len) == 0)
			return __UNCONST(estr);
	return NULL;
}

static void
who_am_i(const char *fname, int show_labels)
{
	struct passwd *pw;
	const char *p;
	char *t;
	time_t now;
	struct utmpentry *ehead, *ep;

	/* search through the utmp and find an entry for this tty */
	if ((p = ttyname(STDIN_FILENO)) != NULL) {

		/* strip directory prefixes for ttys */
		if ((t = strrstr(p, "/pts/")) != NULL ||
		    (t = strrchr(p, '/')) != NULL)
			p = t + 1;

		(void)getutentries(fname, &ehead);
		for (ep = ehead; ep; ep = ep->next)
			if (strcmp(ep->line, p) == 0) {
				if (show_labels)
					output_labels();
				eprint(ep);
				return;
			}
	} else
		p = "tty??";

	(void)time(&now);
	pw = getpwuid(getuid());
	if (show_labels)
		output_labels();
	print(pw ? pw->pw_name : "?", p, now, "", getpid(), 0, 0, 0, 0);
}

static void
process(const char *fname, int show_labels)
{
	struct utmpentry *ehead, *ep;
	(void)getutentries(fname, &ehead);
	if (show_labels)
		output_labels();
	for (ep = ehead; ep != NULL; ep = ep->next)
		eprint(ep);
}

static void
eprint(const struct utmpentry *ep)
{
	print(ep->name, ep->line, (time_t)ep->tv.tv_sec, ep->host, ep->pid,
	    ep->term, ep->exit, ep->sess, ep->type);
}

static void
print(const char *name, const char *line, time_t t, const char *host,
    pid_t pid, uint16_t term, uint16_t xit, uint16_t sess, uint16_t type)
{
	struct stat sb;
	char state;
	static time_t now = 0;
	time_t idle;
	const char *types = NULL;
	size_t i;
	char *tstr;

	state = '?';
	idle = 0;

	for (i = 0; ut_type_names[i].type >= 0; i++) {
		types = ut_type_names[i].name;
		if (ut_type_names[i].type == type)
			break;
	}
	
	if (show_term || show_idle) {
		if (now == 0)
			time(&now);
		
		if (stat(line, &sb) == 0) {
			state = (sb.st_mode & 020) ? '+' : '-';
			idle = now - sb.st_atime;
		}
		
	}

	(void)printf("%-*.*s ", maxname, maxname, name);

	if (show_term)
		(void)printf("%c ", state);

	(void)printf("%-*.*s ", maxline, maxline, line);
	tstr = ctime(&t);
	(void)printf("%.12s ", tstr ? tstr + 4 : "?");

	if (show_idle) {
		if (idle < 60) 
			(void)printf("  .   ");
		else if (idle < (24 * 60 * 60))
			(void)printf("%02ld:%02ld ", 
				     (long)(idle / (60 * 60)),
				     (long)(idle % (60 * 60)) / 60);
		else
			(void)printf(" old  ");

		(void)printf("\t%6d", pid);
		
		if (show_details) {
			if (type == RUN_LVL)
				(void)printf("\tnew=%c old=%c", term, xit);
			else
				(void)printf("\tterm=%d exit=%d", term, xit);
			(void)printf(" sess=%d", sess);
			(void)printf(" type=%s ", types);
		}
	}
	
	if (*host)
		(void)printf("\t(%.*s)", maxhost, host);
	(void)putchar('\n');
}

static void
output_labels(void)
{
	(void)printf("%-*.*s ", maxname, maxname, "USER");

	if (show_term)
		(void)printf("S ");
	
	(void)printf("%-*.*s ", maxline, maxline, "LINE");
	(void)printf("WHEN         ");

	if (show_idle) {
		(void)printf("IDLE  ");
		(void)printf("\t   PID");
	
		(void)printf("\tCOMMENT");
	}		

	(void)putchar('\n');
}

static void
quick(const char *fname)
{
	struct utmpentry *ehead, *ep;
	int num = 0;

	(void)getutentries(fname, &ehead);
	for (ep = ehead; ep != NULL; ep = ep->next) {
		(void)printf("%-*s ", maxname, ep->name);
		if ((++num % 8) == 0)
			(void)putchar('\n');
	}
	if (num % 8)
		(void)putchar('\n');

	(void)printf("# users = %d\n", num);
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-abdHlmqrsTtuv] [file]\n\t%s am i\n",
	    getprogname(), getprogname());
	exit(EXIT_FAILURE);
}
