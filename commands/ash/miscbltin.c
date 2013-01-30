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
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)miscbltin.c	8.4 (Berkeley) 5/4/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
/*
__FBSDID("$FreeBSD: src/bin/sh/miscbltin.c,v 1.30 2004/04/06 20:06:51 markm Exp $");
*/

/*
 * Miscellaneous builtins.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include "shell.h"
#include "options.h"
#include "var.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"
#include "builtins.h"

#undef eflag

/*
 * The read builtin.  The -r option causes backslashes to be treated like
 * ordinary characters.
 *
 * This uses unbuffered input, which may be avoidable in some cases.
 */

int
readcmd(int argc __unused, char **argv __unused)
{
	char **ap;
	int backslash;
	char c;
	int rflag;
	char *prompt;
	char *ifs;
	char *p;
	int startword;
	int status;
	int i;
	struct timeval tv;
	char *tvptr;
#ifndef __minix
	fd_set ifds;
	struct termios told, tnew;
	int tsaved;
#endif

	rflag = 0;
	prompt = NULL;
	tv.tv_sec = -1;
	tv.tv_usec = 0;
	while ((i = nextopt("erp:t:")) != '\0') {
		switch(i) {
		case 'p':
			prompt = shoptarg;
			break;
		case 'e':
			break;
		case 'r':
			rflag = 1;
			break;
		case 't':
			tv.tv_sec = strtol(shoptarg, &tvptr, 0);
			if (tvptr == shoptarg)
				error("timeout value");
			switch(*tvptr) {
			case 0:
			case 's':
				break;
			case 'h':
				tv.tv_sec *= 60;
				/* FALLTHROUGH */
			case 'm':
				tv.tv_sec *= 60;
				break;
			default:
				error("timeout unit");
			}
			break;
		}
	}
	if (prompt && isatty(0)) {
		out2str(prompt);
		flushall();
	}
	if (*(ap = argptr) == NULL)
		error("arg count");
	if ((ifs = bltinlookup("IFS", 1)) == NULL)
		ifs = nullstr;

	if (tv.tv_sec >= 0) {
#ifdef __minix
		abort();
#else
		/*
		 * See if we can disable input processing; this will
		 * not give the desired result if we are in a pipeline
		 * and someone upstream is still in line-by-line mode.
		 */
		tsaved = 0;
		if (tcgetattr(0, &told) == 0) {
			memcpy(&tnew, &told, sizeof(told));
			cfmakeraw(&tnew);
			tcsetattr(0, TCSANOW, &tnew);
			tsaved = 1;
		}
		/*
		 * Wait for something to become available.
		 */
		FD_ZERO(&ifds);
		FD_SET(0, &ifds);
		status = select(1, &ifds, NULL, NULL, &tv);
		if (tsaved)
			tcsetattr(0, TCSANOW, &told);
		/*
		 * If there's nothing ready, return an error.
		 */
		if (status <= 0)
			return(1);
#endif
	}

	status = 0;
	startword = 1;
	backslash = 0;
	STARTSTACKSTR(p);
	for (;;) {
		if (read(STDIN_FILENO, &c, 1) != 1) {
			status = 1;
			break;
		}
		if (c == '\0')
			continue;
		if (backslash) {
			backslash = 0;
			if (c != '\n')
				STPUTC(c, p);
			continue;
		}
		if (!rflag && c == '\\') {
			backslash++;
			continue;
		}
		if (c == '\n')
			break;
		if (startword && *ifs == ' ' && strchr(ifs, c)) {
			continue;
		}
		startword = 0;
		if (backslash && c == '\\') {
			if (read(STDIN_FILENO, &c, 1) != 1) {
				status = 1;
				break;
			}
			STPUTC(c, p);
		} else if (ap[1] != NULL && strchr(ifs, c) != NULL) {
			STACKSTRNUL(p);
			setvar(*ap, stackblock(), 0);
			ap++;
			startword = 1;
			STARTSTACKSTR(p);
		} else {
			STPUTC(c, p);
		}
	}
	STACKSTRNUL(p);
	setvar(*ap, stackblock(), 0);
	while (*++ap != NULL)
		setvar(*ap, nullstr, 0);
	return status;
}



int
umaskcmd(int argc __unused, char **argv)
{
	char *ap;
	int mask;
	int i;
	int symbolic_mode = 0;

	while ((i = nextopt("S")) != '\0') {
		symbolic_mode = 1;
	}

	INTOFF;
	mask = umask(0);
	umask(mask);
	INTON;

	if ((ap = *argptr) == NULL) {
		if (symbolic_mode) {
			char u[4], g[4], o[4];

			i = 0;
			if ((mask & S_IRUSR) == 0)
				u[i++] = 'r';
			if ((mask & S_IWUSR) == 0)
				u[i++] = 'w';
			if ((mask & S_IXUSR) == 0)
				u[i++] = 'x';
			u[i] = '\0';

			i = 0;
			if ((mask & S_IRGRP) == 0)
				g[i++] = 'r';
			if ((mask & S_IWGRP) == 0)
				g[i++] = 'w';
			if ((mask & S_IXGRP) == 0)
				g[i++] = 'x';
			g[i] = '\0';

			i = 0;
			if ((mask & S_IROTH) == 0)
				o[i++] = 'r';
			if ((mask & S_IWOTH) == 0)
				o[i++] = 'w';
			if ((mask & S_IXOTH) == 0)
				o[i++] = 'x';
			o[i] = '\0';

			out1fmt("u=%s,g=%s,o=%s\n", u, g, o);
		} else {
			out1fmt("%.4o\n", mask);
		}
	} else {
		if (isdigit(*ap)) {
			mask = 0;
			do {
				if (*ap >= '8' || *ap < '0')
					error("Illegal number: %s", argv[1]);
				mask = (mask << 3) + (*ap - '0');
			} while (*++ap != '\0');
			umask(mask);
		} else {
			void *set;
			if ((set = setmode (ap)) == 0)
				error("Illegal number: %s", ap);

			mask = getmode (set, ~mask & 0777);
			umask(~mask & 0777);
			free(set);
		}
	}
	return 0;
}

#ifdef __minix
struct rlimit
{
        unsigned long  rlim_cur;	/* current (soft) limit */
	unsigned long  rlim_max;	/* maximum value for rlim_cur */
};
#define RLIM_INFINITY   (((unsigned long)1 << 31) - 1)

int getrlimit (int, struct rlimit *);
int setrlimit (int, const struct rlimit *);

int getrlimit(resource, rlp)
int resource;
struct rlimit *rlp;
{
	errno= ENOSYS;
	return -1;
}
int setrlimit(resource, rlp)
int resource;
const struct rlimit *rlp;
{
	errno= ENOSYS;
	return -1;
}
#endif

/*
 * ulimit builtin
 *
 * This code, originally by Doug Gwyn, Doug Kingston, Eric Gisin, and
 * Michael Rendell was ripped from pdksh 5.0.8 and hacked for use with
 * ash by J.T. Conklin.
 *
 * Public domain.
 */

struct limits {
	const char *name;
	const char *units;
	int	cmd;
	int	factor;	/* multiply by to get rlim_{cur,max} values */
	char	option;
};

static const struct limits limits[] = {
#ifdef RLIMIT_CPU
	{ "cpu time",		"seconds",	RLIMIT_CPU,	   1, 't' },
#endif
#ifdef RLIMIT_FSIZE
	{ "file size",		"512-blocks",	RLIMIT_FSIZE,	 512, 'f' },
#endif
#ifdef RLIMIT_DATA
	{ "data seg size",	"kbytes",	RLIMIT_DATA,	1024, 'd' },
#endif
#ifdef RLIMIT_STACK
	{ "stack size",		"kbytes",	RLIMIT_STACK,	1024, 's' },
#endif
#ifdef  RLIMIT_CORE
	{ "core file size",	"512-blocks",	RLIMIT_CORE,	 512, 'c' },
#endif
#ifdef RLIMIT_RSS
	{ "max memory size",	"kbytes",	RLIMIT_RSS,	1024, 'm' },
#endif
#ifdef RLIMIT_MEMLOCK
	{ "locked memory",	"kbytes",	RLIMIT_MEMLOCK, 1024, 'l' },
#endif
#ifdef RLIMIT_NPROC
	{ "max user processes",	(char *)0,	RLIMIT_NPROC,      1, 'u' },
#endif
#ifdef RLIMIT_NOFILE
	{ "open files",		(char *)0,	RLIMIT_NOFILE,     1, 'n' },
#endif
#ifdef RLIMIT_VMEM
	{ "virtual mem size",	"kbytes",	RLIMIT_VMEM,	1024, 'v' },
#endif
#ifdef RLIMIT_SWAP
	{ "swap limit",		"kbytes",	RLIMIT_SWAP,	1024, 'w' },
#endif
#ifdef RLIMIT_SBSIZE
	{ "sbsize",		"bytes",	RLIMIT_SBSIZE,	   1, 'b' },
#endif
	{ (char *) 0,		(char *)0,	0,		   0, '\0' }
};

int
ulimitcmd(int argc __unused, char **argv __unused)
{
	int	c;
	intmax_t val = 0;
	enum { SOFT = 0x1, HARD = 0x2 }
			how = SOFT | HARD;
	const struct limits	*l;
	int		set, all = 0;
	int		optc, what;
	struct rlimit	limit;

	what = 'f';
	while ((optc = nextopt("HSatfdsmcnuvlb")) != '\0')
		switch (optc) {
		case 'H':
			how = HARD;
			break;
		case 'S':
			how = SOFT;
			break;
		case 'a':
			all = 1;
			break;
		default:
			what = optc;
		}

	for (l = limits; l->name && l->option != what; l++)
		;
	if (!l->name)
		error("internal error (%c)", what);

	set = *argptr ? 1 : 0;
	if (set) {
		char *p = *argptr;

		if (all || argptr[1])
			error("too many arguments");
		if (strcmp(p, "unlimited") == 0)
			val = RLIM_INFINITY;
		else {
			val = 0;

			while ((c = *p++) >= '0' && c <= '9')
			{
				val = (val * 10) + (long)(c - '0');
				if (val < 0)
					break;
			}
			if (c)
				error("bad number");
			val *= l->factor;
		}
	}
	if (all) {
		for (l = limits; l->name; l++) { 
			char optbuf[40];
			if (getrlimit(l->cmd, &limit) < 0)
				error("can't get limit: %s", strerror(errno));
			if (how & SOFT)
				val = limit.rlim_cur;
			else if (how & HARD)
				val = limit.rlim_max;

			if (l->units)
				snprintf(optbuf, sizeof(optbuf),
					"(%s, -%c) ", l->units, l->option);
			else
				snprintf(optbuf, sizeof(optbuf),
					"(-%c) ", l->option);
			out1fmt("%-18s %18s ", l->name, optbuf);
			if (val == RLIM_INFINITY)
				out1fmt("unlimited\n");
			else
			{
				val /= l->factor;
				out1fmt("%jd\n", (intmax_t)val);
			}
		}
		return 0;
	}

	if (getrlimit(l->cmd, &limit) < 0)
		error("can't get limit: %s", strerror(errno));
	if (set) {
		if (how & SOFT)
			limit.rlim_cur = val;
		if (how & HARD)
			limit.rlim_max = val;
		if (setrlimit(l->cmd, &limit) < 0)
			error("bad limit: %s", strerror(errno));
	} else {
		if (how & SOFT)
			val = limit.rlim_cur;
		else if (how & HARD)
			val = limit.rlim_max;

		if (val == RLIM_INFINITY)
			out1fmt("unlimited\n");
		else
		{
			val /= l->factor;
			out1fmt("%jd\n", (intmax_t)val);
		}
	}
	return 0;
}

/*
 * $PchId: miscbltin.c,v 1.7 2006/05/23 11:59:08 philip Exp $
 */
