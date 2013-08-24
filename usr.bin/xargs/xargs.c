/*	$NetBSD: xargs.c,v 1.20 2010/12/17 11:32:57 plunky Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John B. Roll Jr.
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
 *
 * $xMach: xargs.c,v 1.6 2002/02/23 05:27:47 tim Exp $
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1990, 1993\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)xargs.c	8.1 (Berkeley) 6/6/93";
__FBSDID("$FreeBSD: src/usr.bin/xargs/xargs.c,v 1.62 2006/01/01 22:59:54 jmallett Exp $");
#endif
__RCSID("$NetBSD: xargs.c,v 1.20 2010/12/17 11:32:57 plunky Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <paths.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "pathnames.h"

static void	parse_input(int, char *[]);
static void	prerun(int, char *[]);
static int	prompt(void);
static void	run(char **);
static void	usage(void) __dead;
void		strnsubst(char **, const char *, const char *, size_t);
static void	waitchildren(const char *, int);

static char echo[] = _PATH_ECHO;
static char **av, **bxp, **ep, **endxp, **xp;
static char *argp, *bbp, *ebp, *inpline, *p, *replstr;
static const char *eofstr;
static int count, insingle, indouble, oflag, pflag, tflag, Rflag, rval, zflag;
static int cnt, Iflag, jfound, Lflag, Sflag, wasquoted, xflag;
static int curprocs, maxprocs;

static volatile int childerr;

extern char **environ;

int
main(int argc, char *argv[])
{
	long arg_max;
	int ch, Jflag, nargs, nflag, nline;
	size_t linelen;
	char *endptr;

	setprogname(argv[0]);

	inpline = replstr = NULL;
	ep = environ;
	eofstr = "";
	Jflag = nflag = 0;

	(void)setlocale(LC_ALL, "");

	/*
	 * SUSv3 says of the exec family of functions:
	 *     The number of bytes available for the new process'
	 *     combined argument and environment lists is {ARG_MAX}. It
	 *     is implementation-defined whether null terminators,
	 *     pointers, and/or any alignment bytes are included in this
	 *     total.
	 *
	 * SUSv3 says of xargs:
	 *     ... the combined argument and environment lists ...
	 *     shall not exceed {ARG_MAX}-2048.
	 *
	 * To be conservative, we use ARG_MAX - 4K, and we do include
	 * nul terminators and pointers in the calculation.
	 *
	 * Given that the smallest argument is 2 bytes in length, this
	 * means that the number of arguments is limited to:
	 *
	 *	 (ARG_MAX - 4K - LENGTH(env + utility + arguments)) / 2.
	 *
	 * We arbitrarily limit the number of arguments to 5000.  This is
	 * allowed by POSIX.2 as long as the resulting minimum exec line is
	 * at least LINE_MAX.  Realloc'ing as necessary is possible, but
	 * probably not worthwhile.
	 */
	nargs = 5000;
	if ((arg_max = sysconf(_SC_ARG_MAX)) == -1)
		errx(1, "sysconf(_SC_ARG_MAX) failed");
	nline = arg_max - 4 * 1024;
	while (*ep != NULL) {
		/* 1 byte for each '\0' */
		nline -= strlen(*ep++) + 1 + sizeof(*ep);
	}
	maxprocs = 1;
	while ((ch = getopt(argc, argv, "0E:I:J:L:n:oP:pR:S:s:rtx")) != -1)
		switch (ch) {
		case 'E':
			eofstr = optarg;
			break;
		case 'I':
			Jflag = 0;
			Iflag = 1;
			Lflag = 1;
			replstr = optarg;
			break;
		case 'J':
			Iflag = 0;
			Jflag = 1;
			replstr = optarg;
			break;
		case 'L':
			Lflag = atoi(optarg);
			break;
		case 'n':
			nflag = 1;
			if ((nargs = atoi(optarg)) <= 0)
				errx(1, "illegal argument count");
			break;
		case 'o':
			oflag = 1;
			break;
		case 'P':
			if ((maxprocs = atoi(optarg)) <= 0)
				errx(1, "max. processes must be >0");
			break;
		case 'p':
			pflag = 1;
			break;
		case 'R':
			Rflag = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				errx(1, "replacements must be a number");
			break;
		case 'r':
			/* GNU compatibility */
			break;
		case 'S':
			Sflag = strtoul(optarg, &endptr, 10);
			if (*endptr != '\0')
				errx(1, "replsize must be a number");
			break;
		case 's':
			nline = atoi(optarg);
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case '0':
			zflag = 1;
			break;
		case '?':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (!Iflag && Rflag)
		usage();
	if (!Iflag && Sflag)
		usage();
	if (Iflag && !Rflag)
		Rflag = 5;
	if (Iflag && !Sflag)
		Sflag = 255;
	if (xflag && !nflag)
		usage();
	if (Iflag || Lflag)
		xflag = 1;
	if (replstr != NULL && *replstr == '\0')
		errx(1, "replstr may not be empty");

	/*
	 * Allocate pointers for the utility name, the utility arguments,
	 * the maximum arguments to be read from stdin and the trailing
	 * NULL.
	 */
	linelen = 1 + argc + nargs + 1;
	if ((av = bxp = malloc(linelen * sizeof(char **))) == NULL)
		errx(1, "malloc failed");

	/*
	 * Use the user's name for the utility as argv[0], just like the
	 * shell.  Echo is the default.  Set up pointers for the user's
	 * arguments.
	 */
	if (*argv == NULL)
		cnt = strlen(*bxp++ = echo);
	else {
		do {
			if (Jflag && strcmp(*argv, replstr) == 0) {
				char **avj;
				jfound = 1;
				argv++;
				for (avj = argv; *avj; avj++)
					cnt += strlen(*avj) + 1;
				break;
			}
			cnt += strlen(*bxp++ = *argv) + 1;
		} while (*++argv != NULL);
	}

	/*
	 * Set up begin/end/traversing pointers into the array.  The -n
	 * count doesn't include the trailing NULL pointer, so the malloc
	 * added in an extra slot.
	 */
	endxp = (xp = bxp) + nargs;

	/*
	 * Allocate buffer space for the arguments read from stdin and the
	 * trailing NULL.  Buffer space is defined as the default or specified
	 * space, minus the length of the utility name and arguments.  Set up
	 * begin/end/traversing pointers into the array.  The -s count does
	 * include the trailing NULL, so the malloc didn't add in an extra
	 * slot.
	 */
	nline -= cnt;
	if (nline <= 0)
		errx(1, "insufficient space for command");

	if ((bbp = malloc((size_t)(nline + 1))) == NULL)
		errx(1, "malloc failed");
	ebp = (argp = p = bbp) + nline - 1;
	for (;;)
		parse_input(argc, argv);
}

static void
parse_input(int argc, char *argv[])
{
	int ch, foundeof;
	char **avj;

	foundeof = 0;

	switch (ch = getchar()) {
	case EOF:
		/* No arguments since last exec. */
		if (p == bbp) {
			waitchildren(*argv, 1);
			exit(rval);
		}
		goto arg1;
	case ' ':
	case '\t':
		/* Quotes escape tabs and spaces. */
		if (insingle || indouble || zflag)
			goto addch;
		goto arg2;
	case '\0':
		if (zflag) {
			/*
			 * Increment 'count', so that nulls will be treated
			 * as end-of-line, as well as end-of-argument.  This
			 * is needed so -0 works properly with -I and -L.
			 */
			count++;
			goto arg2;
		}
		goto addch;
	case '\n':
		if (zflag)
			goto addch;
		count++;	    /* Indicate end-of-line (used by -L) */

		/* Quotes do not escape newlines. */
arg1:		if (insingle || indouble)
			errx(1, "unterminated quote");
arg2:
		foundeof = *eofstr != '\0' &&
		    strncmp(argp, eofstr, (size_t)(p - argp)) == 0;

		/* Do not make empty args unless they are quoted */
		if ((argp != p || wasquoted) && !foundeof) {
			*p++ = '\0';
			*xp++ = argp;
			if (Iflag) {
				size_t curlen;

				if (inpline == NULL)
					curlen = 0;
				else {
					/*
					 * If this string is not zero
					 * length, append a space for
					 * separation before the next
					 * argument.
					 */
					if ((curlen = strlen(inpline)) != 0)
						(void)strcat(inpline, " ");
				}
				curlen++;
				/*
				 * Allocate enough to hold what we will
				 * be holding in a second, and to append
				 * a space next time through, if we have
				 * to.
				 */
				inpline = realloc(inpline, curlen + 2 +
				    strlen(argp));
				if (inpline == NULL)
					errx(1, "realloc failed");
				if (curlen == 1)
					(void)strcpy(inpline, argp);
				else
					(void)strcat(inpline, argp);
			}
		}

		/*
		 * If max'd out on args or buffer, or reached EOF,
		 * run the command.  If xflag and max'd out on buffer
		 * but not on args, object.  Having reached the limit
		 * of input lines, as specified by -L is the same as
		 * maxing out on arguments.
		 */
		if (xp == endxp || p > ebp || ch == EOF ||
		    (Lflag <= count && xflag) || foundeof) {
			if (xflag && xp != endxp && p > ebp)
				errx(1, "insufficient space for arguments");
			if (jfound) {
				for (avj = argv; *avj; avj++)
					*xp++ = *avj;
			}
			prerun(argc, av);
			if (ch == EOF || foundeof) {
				waitchildren(*argv, 1);
				exit(rval);
			}
			p = bbp;
			xp = bxp;
			count = 0;
		}
		argp = p;
		wasquoted = 0;
		break;
	case '\'':
		if (indouble || zflag)
			goto addch;
		insingle = !insingle;
		wasquoted = 1;
		break;
	case '"':
		if (insingle || zflag)
			goto addch;
		indouble = !indouble;
		wasquoted = 1;
		break;
	case '\\':
		if (zflag)
			goto addch;
		/* Backslash escapes anything, is escaped by quotes. */
		if (!insingle && !indouble && (ch = getchar()) == EOF)
			errx(1, "backslash at EOF");
		/* FALLTHROUGH */
	default:
addch:		if (p < ebp) {
			*p++ = ch;
			break;
		}

		/* If only one argument, not enough buffer space. */
		if (bxp == xp)
			errx(1, "insufficient space for argument");
		/* Didn't hit argument limit, so if xflag object. */
		if (xflag)
			errx(1, "insufficient space for arguments");

		if (jfound) {
			for (avj = argv; *avj; avj++)
				*xp++ = *avj;
		}
		prerun(argc, av);
		xp = bxp;
		cnt = ebp - argp;
		(void)memcpy(bbp, argp, (size_t)cnt);
		p = (argp = bbp) + cnt;
		*p++ = ch;
		break;
	}
}

/*
 * Do things necessary before run()'ing, such as -I substitution,
 * and then call run().
 */
static void
prerun(int argc, char *argv[])
{
	char **tmp, **tmp2, **avj;
	int repls;

	repls = Rflag;

	if (argc == 0 || repls == 0) {
		*xp = NULL;
		run(argv);
		return;
	}

	avj = argv;

	/*
	 * Allocate memory to hold the argument list, and
	 * a NULL at the tail.
	 */
	tmp = malloc((argc + 1) * sizeof(char**));
	if (tmp == NULL)
		errx(1, "malloc failed");
	tmp2 = tmp;

	/*
	 * Save the first argument and iterate over it, we
	 * cannot do strnsubst() to it.
	 */
	if ((*tmp++ = strdup(*avj++)) == NULL)
		errx(1, "strdup failed");

	/*
	 * For each argument to utility, if we have not used up
	 * the number of replacements we are allowed to do, and
	 * if the argument contains at least one occurrence of
	 * replstr, call strnsubst(), else just save the string.
	 * Iterations over elements of avj and tmp are done
	 * where appropriate.
	 */
	while (--argc) {
		*tmp = *avj++;
		if (repls && strstr(*tmp, replstr) != NULL) {
			strnsubst(tmp++, replstr, inpline, (size_t)Sflag);
			if (repls > 0)
				repls--;
		} else {
			if ((*tmp = strdup(*tmp)) == NULL)
				errx(1, "strdup failed");
			tmp++;
		}
	}

	/*
	 * Run it.
	 */
	*tmp = NULL;
	run(tmp2);

	/*
	 * Walk from the tail to the head, free along the way.
	 */
	for (; tmp2 != tmp; tmp--)
		free(*tmp);
	/*
	 * Now free the list itself.
	 */
	free(tmp2);

	/*
	 * Free the input line buffer, if we have one.
	 */
	if (inpline != NULL) {
		free(inpline);
		inpline = NULL;
	}
}

static void
run(char **argv)
{
	int fd;
	char **avec;

	/*
	 * If the user wants to be notified of each command before it is
	 * executed, notify them.  If they want the notification to be
	 * followed by a prompt, then prompt them.
	 */
	if (tflag || pflag) {
		(void)fprintf(stderr, "%s", *argv);
		for (avec = argv + 1; *avec != NULL; ++avec)
			(void)fprintf(stderr, " %s", *avec);
		/*
		 * If the user has asked to be prompted, do so.
		 */
		if (pflag)
			/*
			 * If they asked not to exec, return without execution
			 * but if they asked to, go to the execution.  If we
			 * could not open their tty, break the switch and drop
			 * back to -t behaviour.
			 */
			switch (prompt()) {
			case 0:
				return;
			case 1:
				goto exec;
			case 2:
				break;
			}
		(void)fprintf(stderr, "\n");
		(void)fflush(stderr);
	}
exec:
	childerr = 0;
	switch (vfork()) {
	case -1:
		err(1, "vfork");
		/*NOTREACHED*/
	case 0:
		if (oflag) {
			if ((fd = open(_PATH_TTY, O_RDONLY)) == -1)
				err(1, "can't open /dev/tty");
		} else {
			fd = open(_PATH_DEVNULL, O_RDONLY);
		}
		if (fd > STDIN_FILENO) {
			if (dup2(fd, STDIN_FILENO) != 0)
				err(1, "can't dup2 to stdin");
			(void)close(fd);
		}
		(void)execvp(argv[0], argv);
		childerr = errno;
		_exit(1);
	}
	curprocs++;
	waitchildren(*argv, 0);
}

static void
waitchildren(const char *name, int waitall)
{
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, !waitall && curprocs < maxprocs ?
	    WNOHANG : 0)) > 0) {
		curprocs--;
		/* If we couldn't invoke the utility, exit. */
		if (childerr != 0) {
			errno = childerr;
			err(errno == ENOENT ? 127 : 126, "%s", name);
		}
		/*
		 * According to POSIX, we have to exit if the utility exits
		 * with a 255 status, or is interrupted by a signal. xargs
		 * is allowed to return any exit status between 1 and 125
		 * in these cases, but we'll use 124 and 125, the same
		 * values used by GNU xargs.
		 */
		if (WIFEXITED(status)) {
			if (WEXITSTATUS (status) == 255) {
				warnx ("%s exited with status 255", name);
				exit(124);
			} else if (WEXITSTATUS (status) != 0) {
				rval = 123;
			}
		} else if (WIFSIGNALED (status)) {
			if (WTERMSIG(status) < NSIG) {
				warnx("%s terminated by SIG%s", name,
				    sys_signame[WTERMSIG(status)]);
			} else {
				warnx("%s terminated by signal %d", name,
				    WTERMSIG(status));
			}
			exit(125);
		}
	}
	if (pid == -1 && errno != ECHILD)
		err(1, "waitpid");
}

/*
 * Prompt the user about running a command.
 */
static int
prompt(void)
{
	regex_t cre;
	size_t rsize;
	int match;
	char *response;
	FILE *ttyfp;

	if ((ttyfp = fopen(_PATH_TTY, "r")) == NULL)
		return (2);	/* Indicate that the TTY failed to open. */
	(void)fprintf(stderr, "?...");
	(void)fflush(stderr);
	if ((response = fgetln(ttyfp, &rsize)) == NULL ||
	    regcomp(&cre, nl_langinfo(YESEXPR), REG_BASIC) != 0) {
		(void)fclose(ttyfp);
		return (0);
	}
	response[rsize - 1] = '\0';
	match = regexec(&cre, response, 0, NULL, 0);
	(void)fclose(ttyfp);
	regfree(&cre);
	return (match == 0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
"Usage: %s [-0opt] [-E eofstr] [-I replstr [-R replacements] [-S replsize]]\n"
"             [-J replstr] [-L number] [-n number [-x]] [-P maxprocs]\n"
"             [-s size] [utility [argument ...]]\n", getprogname());
	exit(1);
}
