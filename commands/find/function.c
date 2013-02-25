/*	$NetBSD: function.c,v 1.64 2007/07/19 07:49:30 daniel Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>
#include <pwd.h>

#include "find.h"

typedef int bool;
#define false 0
#define true 1

#define	COMPARE(a, b) {							\
	switch (plan->flags) {						\
	case F_EQUAL:							\
		return (a == b);					\
	case F_LESSTHAN:						\
		return (a < b);						\
	case F_GREATER:							\
		return (a > b);						\
	default:							\
		abort();						\
	}								\
}

static	int32_t	find_parsenum(PLAN *, const char *, const char *, char *);
static	void	run_f_exec(PLAN *);
	int	f_always_true(PLAN *, FTSENT *);
	int	f_amin(PLAN *, FTSENT *);
	int	f_anewer(PLAN *, FTSENT *);
	int	f_atime(PLAN *, FTSENT *);
	int	f_cmin(PLAN *, FTSENT *);
	int	f_cnewer(PLAN *, FTSENT *);
	int	f_ctime(PLAN *, FTSENT *);
	int	f_delete(PLAN *, FTSENT *);
	int	f_empty(PLAN *, FTSENT *);
	int	f_exec(PLAN *, FTSENT *);
	int	f_execdir(PLAN *, FTSENT *);
	int	f_false(PLAN *, FTSENT *);
	int	f_flags(PLAN *, FTSENT *);
	int	f_fprint(PLAN *, FTSENT *);
	int	f_fstype(PLAN *, FTSENT *);
	int	f_group(PLAN *, FTSENT *);
	int	f_iname(PLAN *, FTSENT *);
	int	f_inum(PLAN *, FTSENT *);
	int	f_links(PLAN *, FTSENT *);
	int	f_ls(PLAN *, FTSENT *);
	int	f_mindepth(PLAN *, FTSENT *);
	int	f_maxdepth(PLAN *, FTSENT *);
	int	f_mmin(PLAN *, FTSENT *);
	int	f_mtime(PLAN *, FTSENT *);
	int	f_name(PLAN *, FTSENT *);
	int	f_newer(PLAN *, FTSENT *);
	int	f_nogroup(PLAN *, FTSENT *);
	int	f_nouser(PLAN *, FTSENT *);
	int	f_path(PLAN *, FTSENT *);
	int	f_perm(PLAN *, FTSENT *);
	int	f_print(PLAN *, FTSENT *);
	int	f_print0(PLAN *, FTSENT *);
	int	f_printx(PLAN *, FTSENT *);
	int	f_prune(PLAN *, FTSENT *);
	int	f_regex(PLAN *, FTSENT *);
	int	f_size(PLAN *, FTSENT *);
	int	f_type(PLAN *, FTSENT *);
	int	f_user(PLAN *, FTSENT *);
	int	f_not(PLAN *, FTSENT *);
	int	f_or(PLAN *, FTSENT *);
static	PLAN   *c_regex_common(char ***, int, enum ntype, bool);
static	PLAN   *palloc(enum ntype, int (*)(PLAN *, FTSENT *));

extern int dotfd;
extern FTS *tree;
extern time_t now;

/*
 * find_parsenum --
 *	Parse a string of the form [+-]# and return the value.
 */
static int32_t
find_parsenum(PLAN *plan, const char *option, const char *vp, char *endch)
{
	int32_t value;
	const char *str;
	char *endchar; /* Pointer to character ending conversion. */

	/* Determine comparison from leading + or -. */
	str = vp;
	switch (*str) {
	case '+':
		++str;
		plan->flags = F_GREATER;
		break;
	case '-':
		++str;
		plan->flags = F_LESSTHAN;
		break;
	default:
		plan->flags = F_EQUAL;
		break;
	}

	/*
	 * Convert the string with strtol().  Note, if strtol() returns zero
	 * and endchar points to the beginning of the string we know we have
	 * a syntax error.
	 */
	value = strtol(str, &endchar, 10);
	if (value == 0 && endchar == str)
		errx(1, "%s: %s: illegal numeric value", option, vp);
	if (endchar[0] && (endch == NULL || endchar[0] != *endch))
		errx(1, "%s: %s: illegal trailing character", option, vp);
	if (endch)
		*endch = endchar[0];
	return (value);
}

/*
 * The value of n for the inode times (atime, ctime, and mtime) is a range,
 * i.e. n matches from (n - 1) to n 24 hour periods.  This interacts with
 * -n, such that "-mtime -1" would be less than 0 days, which isn't what the
 * user wanted.  Correct so that -1 is "less than 1".
 */
#define	TIME_CORRECT(p, ttype)						\
	if ((p)->type == ttype && (p)->flags == F_LESSTHAN)		\
		++((p)->t_data);

/*
 * -amin n functions --
 *
 *	True if the difference between the file access time and the
 *	current time is n 1 minute periods.
 */
int
f_amin(PLAN *plan, FTSENT *entry)
{
	COMPARE((now - entry->fts_statp->st_atime +
	    SECSPERMIN - 1) / SECSPERMIN, plan->t_data);
}

PLAN *
c_amin(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_AMIN, f_amin);
	new->t_data = find_parsenum(new, "-amin", arg, NULL);
	TIME_CORRECT(new, N_AMIN);
	return (new);
}

/*
 * -anewer file functions --
 *
 *	True if the current file has been accessed more recently
 *	than the access time of the file named by the pathname
 *	file.
 */
int
f_anewer(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{

	return (entry->fts_statp->st_atime > plan->t_data);
}

PLAN *
c_anewer(char ***argvp, int isok)
{
	char *filename = **argvp;
	PLAN *new;
	struct stat sb;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	if (stat(filename, &sb))
		err(1, "%s", filename);
	new = palloc(N_ANEWER, f_anewer);
	new->t_data = sb.st_atime;
	return (new);
}

/*
 * -atime n functions --
 *
 *	True if the difference between the file access time and the
 *	current time is n 24 hour periods.
 */
int
f_atime(PLAN *plan, FTSENT *entry)
{
	COMPARE((now - entry->fts_statp->st_atime +
	    SECSPERDAY - 1) / SECSPERDAY, plan->t_data);
}

PLAN *
c_atime(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_ATIME, f_atime);
	new->t_data = find_parsenum(new, "-atime", arg, NULL);
	TIME_CORRECT(new, N_ATIME);
	return (new);
}
/*
 * -cmin n functions --
 *
 *	True if the difference between the last change of file
 *	status information and the current time is n 24 hour periods.
 */
int
f_cmin(PLAN *plan, FTSENT *entry)
{
	COMPARE((now - entry->fts_statp->st_ctime +
	    SECSPERMIN - 1) / SECSPERMIN, plan->t_data);
}

PLAN *
c_cmin(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_CMIN, f_cmin);
	new->t_data = find_parsenum(new, "-cmin", arg, NULL);
	TIME_CORRECT(new, N_CMIN);
	return (new);
}

/*
 * -cnewer file functions --
 *
 *	True if the current file has been changed more recently
 *	than the changed time of the file named by the pathname
 *	file.
 */
int
f_cnewer(PLAN *plan, FTSENT *entry)
{

	return (entry->fts_statp->st_ctime > plan->t_data);
}

PLAN *
c_cnewer(char ***argvp, int isok)
{
	char *filename = **argvp;
	PLAN *new;
	struct stat sb;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	if (stat(filename, &sb))
		err(1, "%s", filename);
	new = palloc(N_CNEWER, f_cnewer);
	new->t_data = sb.st_ctime;
	return (new);
}

/*
 * -ctime n functions --
 *
 *	True if the difference between the last change of file
 *	status information and the current time is n 24 hour periods.
 */
int
f_ctime(PLAN *plan, FTSENT *entry)
{
	COMPARE((now - entry->fts_statp->st_ctime +
	    SECSPERDAY - 1) / SECSPERDAY, plan->t_data);
}

PLAN *
c_ctime(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_CTIME, f_ctime);
	new->t_data = find_parsenum(new, "-ctime", arg, NULL);
	TIME_CORRECT(new, N_CTIME);
	return (new);
}

/*
 * -delete functions --
 *
 *	True always.  Makes its best shot and continues on regardless.
 */
int
f_delete(PLAN *plan __unused, FTSENT *entry)
{
	/* ignore these from fts */
	if (strcmp(entry->fts_accpath, ".") == 0 ||
	    strcmp(entry->fts_accpath, "..") == 0)
		return 1;

	/* sanity check */
	if (isdepth == 0 ||			/* depth off */
	    (ftsoptions & FTS_NOSTAT) ||	/* not stat()ing */
	    !(ftsoptions & FTS_PHYSICAL) ||	/* physical off */
	    (ftsoptions & FTS_LOGICAL))		/* or finally, logical on */
		errx(1, "-delete: insecure options got turned on");

	/* Potentially unsafe - do not accept relative paths whatsoever */
	if (strchr(entry->fts_accpath, '/') != NULL)
		errx(1, "-delete: %s: relative path potentially not safe",
			entry->fts_accpath);

#if !defined(__minix)
	/* Turn off user immutable bits if running as root */
	if ((entry->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
	    !(entry->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
	    geteuid() == 0)
		chflags(entry->fts_accpath,
		       entry->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE));
#endif

	/* rmdir directories, unlink everything else */
	if (S_ISDIR(entry->fts_statp->st_mode)) {
		if (rmdir(entry->fts_accpath) < 0 && errno != ENOTEMPTY)
			warn("-delete: rmdir(%s)", entry->fts_path);
	} else {
		if (unlink(entry->fts_accpath) < 0)
			warn("-delete: unlink(%s)", entry->fts_path);
	}

	/* "succeed" */
	return 1;
}

PLAN *
c_delete(char ***argvp __unused, int isok)
{

	ftsoptions &= ~FTS_NOSTAT;	/* no optimize */
	ftsoptions |= FTS_PHYSICAL;	/* disable -follow */
	ftsoptions &= ~FTS_LOGICAL;	/* disable -follow */
	isoutput = 1;			/* possible output */
	isdepth = 1;			/* -depth implied */

	return palloc(N_DELETE, f_delete);
}

/*
 * -depth functions --
 *
 *	Always true, causes descent of the directory hierarchy to be done
 *	so that all entries in a directory are acted on before the directory
 *	itself.
 */
int
f_always_true(PLAN *plan, FTSENT *entry)
{

	return (1);
}

PLAN *
c_depth(char ***argvp, int isok)
{
	isdepth = 1;

	return (palloc(N_DEPTH, f_always_true));
}

/*
 * -empty functions --
 *
 *	True if the file or directory is empty
 */
int
f_empty(PLAN *plan, FTSENT *entry)
{
	if (S_ISREG(entry->fts_statp->st_mode) &&
	    entry->fts_statp->st_size == 0)
		return (1);
	if (S_ISDIR(entry->fts_statp->st_mode)) {
		struct dirent *dp;
		int empty;
		DIR *dir;

		empty = 1;
		dir = opendir(entry->fts_accpath);
		if (dir == NULL)
			err(1, "%s", entry->fts_accpath);
		for (dp = readdir(dir); dp; dp = readdir(dir))
			if (dp->d_name[0] != '.' ||
			    (dp->d_name[1] != '\0' &&
				(dp->d_name[1] != '.' || dp->d_name[2] != '\0'))) {
				empty = 0;
				break;
			}
		closedir(dir);
		return (empty);
	}
	return (0);
}

PLAN *
c_empty(char ***argvp, int isok)
{
	ftsoptions &= ~FTS_NOSTAT;

	return (palloc(N_EMPTY, f_empty));
}

/*
 * [-exec | -ok] utility [arg ... ] ; functions --
 * [-exec | -ok] utility [arg ... ] {} + functions --
 *
 *	If the end of the primary expression is delimited by a
 *	semicolon: true if the executed utility returns a zero value
 *	as exit status.  If "{}" occurs anywhere, it gets replaced by
 *	the current pathname.
 *
 *	If the end of the primary expression is delimited by a plus
 *	sign: always true. Pathnames for which the primary is
 *	evaluated shall be aggregated into sets. The utility will be
 *	executed once per set, with "{}" replaced by the entire set of
 *	pathnames (as if xargs). "{}" must appear last.
 *
 *	The current directory for the execution of utility is the same
 *	as the current directory when the find utility was started.
 *
 *	The primary -ok is different in that it requests affirmation
 *	of the user before executing the utility.
 */
int
f_exec(PLAN *plan, FTSENT *entry)
{
	int cnt, l;
	pid_t pid;
	int status;

	if (plan->flags & F_PLUSSET) {
		/*
		 * Confirm sufficient buffer space, then copy the path
		 * to the buffer.
		 */
		l = strlen(entry->fts_path);
		if (plan->ep_p + l < plan->ep_ebp) {
			plan->ep_bxp[plan->ep_narg++] =
			    strcpy(plan->ep_p, entry->fts_path);
			plan->ep_p += l + 1;

			if (plan->ep_narg == plan->ep_maxargs)
				run_f_exec(plan);
		} else {
			/*
			 * Without sufficient space to copy in the next
			 * argument, run the command to empty out the
			 * buffer before re-attepting the copy.
			 */
			run_f_exec(plan);
			if ((plan->ep_p + l < plan->ep_ebp)) {
				plan->ep_bxp[plan->ep_narg++]
				    = strcpy(plan->ep_p, entry->fts_path);
				plan->ep_p += l + 1;
			} else
				errx(1, "insufficient space for argument");
		}
		return (1);
	} else {
		for (cnt = 0; plan->e_argv[cnt]; ++cnt)
			if (plan->e_len[cnt])
				brace_subst(plan->e_orig[cnt],
				    &plan->e_argv[cnt],
				    entry->fts_path,
				    &plan->e_len[cnt]);
		if (plan->flags & F_NEEDOK && !queryuser(plan->e_argv))
			return (0);

		/* Don't mix output of command with find output. */
		fflush(stdout);
		fflush(stderr);

		switch (pid = fork()) {
		case -1:
			err(1, "fork");
			/* NOTREACHED */
		case 0:
			if (fchdir(dotfd)) {
				warn("chdir");
				_exit(1);
			}
			execvp(plan->e_argv[0], plan->e_argv);
			warn("%s", plan->e_argv[0]);
			_exit(1);
		}
		pid = waitpid(pid, &status, 0);
		return (pid != -1 && WIFEXITED(status)
		    && !WEXITSTATUS(status));
	}
}

static void
run_f_exec(PLAN *plan)
{
	pid_t pid;
	int rval, status;

	/* Ensure arg list is null terminated. */
	plan->ep_bxp[plan->ep_narg] = NULL;

	/* Don't mix output of command with find output. */
	fflush(stdout);
	fflush(stderr);

	switch (pid = fork()) {
	case -1:
		err(1, "fork");
		/* NOTREACHED */
	case 0:
		if (fchdir(dotfd)) {
			warn("chdir");
			_exit(1);
		}
		execvp(plan->e_argv[0], plan->e_argv);
		warn("%s", plan->e_argv[0]);
		_exit(1);
	}

	/* Clear out the argument list. */
	plan->ep_narg = 0;
	plan->ep_bxp[plan->ep_narg] = NULL;
	/* As well as the argument buffer. */
	plan->ep_p = plan->ep_bbp;
	*plan->ep_p = '\0';

	pid = waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		rval = WEXITSTATUS(status);
	else
		rval = -1;

	/*
	 * If we have a non-zero exit status, preserve it so find(1) can
	 * later exit with it.
	 */
	if (rval)
		plan->ep_rval = rval;
}

/*
 * c_exec --
 *	build three parallel arrays, one with pointers to the strings passed
 *	on the command line, one with (possibly duplicated) pointers to the
 *	argv array, and one with integer values that are lengths of the
 *	strings, but also flags meaning that the string has to be massaged.
 *
 *	If -exec ... {} +, use only the first array, but make it large
 *	enough to hold 5000 args (cf. src/usr.bin/xargs/xargs.c for a
 *	discussion), and then allocate ARG_MAX - 4K of space for args.
 */
PLAN *
c_exec(char ***argvp, int isok)
{
	PLAN *new;			/* node returned */
	int cnt, brace, lastbrace;
	char **argv, **ap, *p;

	isoutput = 1;

	new = palloc(N_EXEC, f_exec);
	if (isok)
		new->flags |= F_NEEDOK;

	/*
	 * Terminate if we encounter an arg exacty equal to ";", or an
	 * arg exacty equal to "+" following an arg exacty equal to
	 * "{}".
	 */
	for (ap = argv = *argvp, brace = 0;; ++ap) {
		if (!*ap)
			errx(1, "%s: no terminating \";\" or \"+\"",
			    isok ? "-ok" : "-exec");
		lastbrace = brace;
		if (strcmp(*ap, "{}") == 0)
			brace = 1;
		if (strcmp(*ap, ";") == 0)
			break;
		if (strcmp(*ap, "+") == 0 && lastbrace) {
			new->flags |= F_PLUSSET;
			break;
		}
	}

	/*
	 * POSIX says -ok ... {} + "need not be supported," and it does
	 * not make much sense anyway.
	 */
	if (new->flags & F_NEEDOK && new->flags & F_PLUSSET)
		errx(1, "-ok: terminating \"+\" not permitted.");

	if (new->flags & F_PLUSSET) {
		u_int c, bufsize;

		cnt = ap - *argvp - 1;			/* units are words */
		new->ep_maxargs = 5000;
		new->e_argv = (char **)emalloc((u_int)(cnt + new->ep_maxargs)
						* sizeof(char **));

		/* We start stuffing arguments after the user's last one. */
		new->ep_bxp = &new->e_argv[cnt];
		new->ep_narg = 0;

		/*
		 * Count up the space of the user's arguments, and
		 * subtract that from what we allocate.
		 */
		for (argv = *argvp, c = 0, cnt = 0;
		     argv < ap;
		     ++argv, ++cnt) {
			c += strlen(*argv) + 1;
			new->e_argv[cnt] = *argv;
		}
		bufsize = ARG_MAX - 4 * 1024 - c;


		/*
		 * Allocate, and then initialize current, base, and
		 * end pointers.
		 */
		new->ep_p = new->ep_bbp = malloc(bufsize + 1);
		new->ep_ebp = new->ep_bbp + bufsize - 1;
		new->ep_rval = 0;
	} else { /* !F_PLUSSET */
		cnt = ap - *argvp + 1;
		new->e_argv = (char **)emalloc((u_int)cnt * sizeof(char *));
		new->e_orig = (char **)emalloc((u_int)cnt * sizeof(char *));
		new->e_len = (int *)emalloc((u_int)cnt * sizeof(int));

		for (argv = *argvp, cnt = 0; argv < ap; ++argv, ++cnt) {
			new->e_orig[cnt] = *argv;
			for (p = *argv; *p; ++p)
				if (p[0] == '{' && p[1] == '}') {
					new->e_argv[cnt] =
						emalloc((u_int)MAXPATHLEN);
					new->e_len[cnt] = MAXPATHLEN;
					break;
				}
			if (!*p) {
				new->e_argv[cnt] = *argv;
				new->e_len[cnt] = 0;
			}
		}
		new->e_orig[cnt] = NULL;
	}

	new->e_argv[cnt] = NULL;
	*argvp = argv + 1;
	return (new);
}

/*
 * -execdir utility [arg ... ] ; functions --
 *
 *	True if the executed utility returns a zero value as exit status.
 *	The end of the primary expression is delimited by a semicolon.  If
 *	"{}" occurs anywhere, it gets replaced by the unqualified pathname.
 *	The current directory for the execution of utility is the same as
 *	the directory where the file lives.
 */
int
f_execdir(PLAN *plan, FTSENT *entry)
{
	int cnt;
	pid_t pid;
	int status;
	char *file;

	/* XXX - if file/dir ends in '/' this will not work -- can it? */
	if ((file = strrchr(entry->fts_path, '/')))
		file++;
	else
		file = entry->fts_path;

	for (cnt = 0; plan->e_argv[cnt]; ++cnt)
		if (plan->e_len[cnt])
			brace_subst(plan->e_orig[cnt], &plan->e_argv[cnt],
			    file, &plan->e_len[cnt]);

	/* don't mix output of command with find output */
	fflush(stdout);
	fflush(stderr);

	switch (pid = fork()) {
	case -1:
		err(1, "fork");
		/* NOTREACHED */
	case 0:
		execvp(plan->e_argv[0], plan->e_argv);
		warn("%s", plan->e_argv[0]);
		_exit(1);
	}
	pid = waitpid(pid, &status, 0);
	return (pid != -1 && WIFEXITED(status) && !WEXITSTATUS(status));
}

/*
 * c_execdir --
 *	build three parallel arrays, one with pointers to the strings passed
 *	on the command line, one with (possibly duplicated) pointers to the
 *	argv array, and one with integer values that are lengths of the
 *	strings, but also flags meaning that the string has to be massaged.
 */
PLAN *
c_execdir(char ***argvp, int isok)
{
	PLAN *new;			/* node returned */
	int cnt;
	char **argv, **ap, *p;

	ftsoptions &= ~FTS_NOSTAT;
	isoutput = 1;

	new = palloc(N_EXECDIR, f_execdir);

	for (ap = argv = *argvp;; ++ap) {
		if (!*ap)
			errx(1,
			    "-execdir: no terminating \";\"");
		if (**ap == ';')
			break;
	}

	cnt = ap - *argvp + 1;
	new->e_argv = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_orig = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_len = (int *)emalloc((u_int)cnt * sizeof(int));

	for (argv = *argvp, cnt = 0; argv < ap; ++argv, ++cnt) {
		new->e_orig[cnt] = *argv;
		for (p = *argv; *p; ++p)
			if (p[0] == '{' && p[1] == '}') {
				new->e_argv[cnt] = emalloc((u_int)MAXPATHLEN);
				new->e_len[cnt] = MAXPATHLEN;
				break;
			}
		if (!*p) {
			new->e_argv[cnt] = *argv;
			new->e_len[cnt] = 0;
		}
	}
	new->e_argv[cnt] = new->e_orig[cnt] = NULL;

	*argvp = argv + 1;
	return (new);
}

PLAN *
c_exit(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	/* not technically true, but otherwise '-print' is implied */
	isoutput = 1;

	new = palloc(N_EXIT, f_always_true);

	if (arg) {
		(*argvp)++;
		new->exit_val = find_parsenum(new, "-exit", arg, NULL);
	} else
		new->exit_val = 0;

	return (new);
}


/*
 * -false function
 */
int
f_false(PLAN *plan, FTSENT *entry)
{

	return (0);
}

PLAN *
c_false(char ***argvp, int isok)
{
	return (palloc(N_FALSE, f_false));
}


#if !defined(__minix)
/*
 * -flags [-]flags functions --
 */
int
f_flags(PLAN *plan, FTSENT *entry)
{
	u_int32_t flags;

	flags = entry->fts_statp->st_flags;
	if (plan->flags == F_ATLEAST)
		return ((plan->f_data | flags) == flags);
	else
		return (flags == plan->f_data);
	/* MINIX has no file flags. */
	return 0;
	/* NOTREACHED */
}

PLAN *
c_flags(char ***argvp, int isok)
{
	char *flags = **argvp;
	PLAN *new;
	u_long flagset;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_FLAGS, f_flags);

	if (*flags == '-') {
		new->flags = F_ATLEAST;
		++flags;
	}

	flagset = 0;
	if ((strcmp(flags, "none") != 0) &&
	    (string_to_flags(&flags, &flagset, NULL) != 0))
		errx(1, "-flags: %s: illegal flags string", flags);
	new->f_data = flagset;
	return (new);
}
#endif

/*
 * -follow functions --
 *
 *	Always true, causes symbolic links to be followed on a global
 *	basis.
 */
PLAN *
c_follow(char ***argvp, int isok)
{
	ftsoptions &= ~FTS_PHYSICAL;
	ftsoptions |= FTS_LOGICAL;

	return (palloc(N_FOLLOW, f_always_true));
}

/* -fprint functions --
 *
 *	Causes the current pathame to be written to the defined output file.
 */
int
f_fprint(PLAN *plan, FTSENT *entry)
{

	if (-1 == fprintf(plan->fprint_file, "%s\n", entry->fts_path))
		warn("fprintf");

	return(1);

	/* no descriptors are closed; they will be closed by
	   operating system when this find command exits.  */
}

PLAN *
c_fprint(char ***argvp, int isok)
{
	PLAN *new;

	isoutput = 1; /* do not assume -print */

	new = palloc(N_FPRINT, f_fprint);

	if (NULL == (new->fprint_file = fopen(**argvp, "w")))
		err(1, "-fprint: %s: cannot create file", **argvp);

	(*argvp)++;
	return (new);
}

/*
 * -fstype functions --
 *
 *	True if the file is of a certain type.
 */
#if !defined(__minix)
int
f_fstype(PLAN *plan, FTSENT *entry)
{
	static dev_t curdev;	/* need a guaranteed illegal dev value */
	static int first = 1;
	struct statvfs sb;
	static short val;
	static char fstype[sizeof(sb.f_fstypename)];
	char *p, save[2];

	memset(&save, 0, sizeof save);	/* XXX gcc */

	/* Only check when we cross mount point. */
	if (first || curdev != entry->fts_statp->st_dev) {
		curdev = entry->fts_statp->st_dev;

		/*
		 * Statfs follows symlinks; find wants the link's file system,
		 * not where it points.
		 */
		if (entry->fts_info == FTS_SL ||
		    entry->fts_info == FTS_SLNONE) {
			if ((p = strrchr(entry->fts_accpath, '/')) != NULL)
				++p;
			else
				p = entry->fts_accpath;
			save[0] = p[0];
			p[0] = '.';
			save[1] = p[1];
			p[1] = '\0';

		} else
			p = NULL;

		if (statvfs(entry->fts_accpath, &sb))
			err(1, "%s", entry->fts_accpath);

		if (p) {
			p[0] = save[0];
			p[1] = save[1];
		}

		first = 0;

		/*
		 * Further tests may need both of these values, so
		 * always copy both of them.
		 */
		val = sb.f_flag;
		strlcpy(fstype, sb.f_fstypename, sizeof(fstype));
	}
	switch (plan->flags) {
	case F_MTFLAG:
		return (val & plan->mt_data);
	case F_MTTYPE:
		return (strncmp(fstype, plan->c_data, sizeof(fstype)) == 0);
	default:
		abort();
	}
}

PLAN *
c_fstype(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_FSTYPE, f_fstype);

	switch (*arg) {
	case 'l':
		if (!strcmp(arg, "local")) {
			new->flags = F_MTFLAG;
			new->mt_data = MNT_LOCAL;
			return (new);
		}
		break;
	case 'r':
		if (!strcmp(arg, "rdonly")) {
			new->flags = F_MTFLAG;
			new->mt_data = MNT_RDONLY;
			return (new);
		}
		break;
	}

	new->flags = F_MTTYPE;
	new->c_data = arg;
	return (new);
}
#endif

/*
 * -group gname functions --
 *
 *	True if the file belongs to the group gname.  If gname is numeric and
 *	an equivalent of the getgrnam() function does not return a valid group
 *	name, gname is taken as a group ID.
 */
int
f_group(PLAN *plan, FTSENT *entry)
{

	return (entry->fts_statp->st_gid == plan->g_data);
}

PLAN *
c_group(char ***argvp, int isok)
{
	char *gname = **argvp;
	PLAN *new;
	struct group *g;
	gid_t gid;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	g = getgrnam(gname);
	if (g == NULL) {
		gid = atoi(gname);
		if (gid == 0 && gname[0] != '0')
			errx(1, "-group: %s: no such group", gname);
	} else
		gid = g->gr_gid;

	new = palloc(N_GROUP, f_group);
	new->g_data = gid;
	return (new);
}

/*
 * -inum n functions --
 *
 *	True if the file has inode # n.
 */
int
f_inum(PLAN *plan, FTSENT *entry)
{

	COMPARE(entry->fts_statp->st_ino, plan->i_data);
}

PLAN *
c_inum(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_INUM, f_inum);
	new->i_data = find_parsenum(new, "-inum", arg, NULL);
	return (new);
}

/*
 * -links n functions --
 *
 *	True if the file has n links.
 */
int
f_links(PLAN *plan, FTSENT *entry)
{

	COMPARE(entry->fts_statp->st_nlink, plan->l_data);
}

PLAN *
c_links(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_LINKS, f_links);
	new->l_data = (nlink_t)find_parsenum(new, "-links", arg, NULL);
	return (new);
}

/*
 * -ls functions --
 *
 *	Always true - prints the current entry to stdout in "ls" format.
 */
int
f_ls(PLAN *plan, FTSENT *entry)
{

	printlong(entry->fts_path, entry->fts_accpath, entry->fts_statp);
	return (1);
}

PLAN *
c_ls(char ***argvp, int isok)
{

	ftsoptions &= ~FTS_NOSTAT;
	isoutput = 1;

	return (palloc(N_LS, f_ls));
}

/*
 * - maxdepth n functions --
 *
 *	True if the current search depth is less than or equal to the
 *	maximum depth specified
 */
int
f_maxdepth(PLAN *plan, FTSENT *entry)
{
	extern FTS *tree;

	if (entry->fts_level >= plan->max_data)
		fts_set(tree, entry, FTS_SKIP);
	return (entry->fts_level <= plan->max_data);
}

PLAN *
c_maxdepth(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	new = palloc(N_MAXDEPTH, f_maxdepth);
	new->max_data = atoi(arg);
	return (new);
}

/*
 * - mindepth n functions --
 *
 *	True if the current search depth is greater than or equal to the
 *	minimum depth specified
 */
int
f_mindepth(PLAN *plan, FTSENT *entry)
{
	return (entry->fts_level >= plan->min_data);
}

PLAN *
c_mindepth(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	new = palloc(N_MINDEPTH, f_mindepth);
	new->min_data = atoi(arg);
	return (new);
}
/*
 * -mmin n functions --
 *
 *	True if the difference between the file modification time and the
 *	current time is n 24 hour periods.
 */
int
f_mmin(PLAN *plan, FTSENT *entry)
{
	COMPARE((now - entry->fts_statp->st_mtime + SECSPERMIN - 1) /
	    SECSPERMIN, plan->t_data);
}

PLAN *
c_mmin(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_MMIN, f_mmin);
	new->t_data = find_parsenum(new, "-mmin", arg, NULL);
	TIME_CORRECT(new, N_MMIN);
	return (new);
}
/*
 * -mtime n functions --
 *
 *	True if the difference between the file modification time and the
 *	current time is n 24 hour periods.
 */
int
f_mtime(PLAN *plan, FTSENT *entry)
{
	COMPARE((now - entry->fts_statp->st_mtime + SECSPERDAY - 1) /
	    SECSPERDAY, plan->t_data);
}

PLAN *
c_mtime(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_MTIME, f_mtime);
	new->t_data = find_parsenum(new, "-mtime", arg, NULL);
	TIME_CORRECT(new, N_MTIME);
	return (new);
}

/*
 * -name functions --
 *
 *	True if the basename of the filename being examined
 *	matches pattern using Pattern Matching Notation S3.14
 */
int
f_name(PLAN *plan, FTSENT *entry)
{

	return (!fnmatch(plan->c_data, entry->fts_name, 0));
}

PLAN *
c_name(char ***argvp, int isok)
{
	char *pattern = **argvp;
	PLAN *new;

	(*argvp)++;
	new = palloc(N_NAME, f_name);
	new->c_data = pattern;
	return (new);
}

/*
 * -iname functions --
 *
 *	Similar to -name, but does case insensitive matching
 *
 */
int
f_iname(PLAN *plan, FTSENT *entry)
{
	return (!fnmatch(plan->c_data, entry->fts_name, FNM_CASEFOLD));
}

PLAN *
c_iname(char ***argvp, int isok)
{
	char *pattern = **argvp;
	PLAN *new;

	(*argvp)++;
	new = palloc(N_INAME, f_iname);
	new->c_data = pattern;
	return (new);
}

/*
 * -newer file functions --
 *
 *	True if the current file has been modified more recently
 *	than the modification time of the file named by the pathname
 *	file.
 */
int
f_newer(PLAN *plan, FTSENT *entry)
{

	return (entry->fts_statp->st_mtime > plan->t_data);
}

PLAN *
c_newer(char ***argvp, int isok)
{
	char *filename = **argvp;
	PLAN *new;
	struct stat sb;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	if (stat(filename, &sb))
		err(1, "%s", filename);
	new = palloc(N_NEWER, f_newer);
	new->t_data = sb.st_mtime;
	return (new);
}

/*
 * -nogroup functions --
 *
 *	True if file belongs to a user ID for which the equivalent
 *	of the getgrnam() 9.2.1 [POSIX.1] function returns NULL.
 */
int
f_nogroup(PLAN *plan, FTSENT *entry)
{

	return (group_from_gid(entry->fts_statp->st_gid, 1) ? 0 : 1);
}

PLAN *
c_nogroup(char ***argvp, int isok)
{
	ftsoptions &= ~FTS_NOSTAT;

	return (palloc(N_NOGROUP, f_nogroup));
}

/*
 * -nouser functions --
 *
 *	True if file belongs to a user ID for which the equivalent
 *	of the getpwuid() 9.2.2 [POSIX.1] function returns NULL.
 */
int
f_nouser(PLAN *plan, FTSENT *entry)
{

	return (user_from_uid(entry->fts_statp->st_uid, 1) ? 0 : 1);
}

PLAN *
c_nouser(char ***argvp, int isok)
{
	ftsoptions &= ~FTS_NOSTAT;

	return (palloc(N_NOUSER, f_nouser));
}

/*
 * -path functions --
 *
 *	True if the path of the filename being examined
 *	matches pattern using Pattern Matching Notation S3.14
 */
int
f_path(PLAN *plan, FTSENT *entry)
{

	return (!fnmatch(plan->c_data, entry->fts_path, 0));
}

PLAN *
c_path(char ***argvp, int isok)
{
	char *pattern = **argvp;
	PLAN *new;

	(*argvp)++;
	new = palloc(N_NAME, f_path);
	new->c_data = pattern;
	return (new);
}

/*
 * -perm functions --
 *
 *	The mode argument is used to represent file mode bits.  If it starts
 *	with a leading digit, it's treated as an octal mode, otherwise as a
 *	symbolic mode.
 */
int
f_perm(PLAN *plan, FTSENT *entry)
{
	mode_t mode;

	mode = entry->fts_statp->st_mode &
	    (S_ISUID|S_ISGID
#ifdef S_ISTXT
	    |S_ISTXT
#endif
	    |S_IRWXU|S_IRWXG|S_IRWXO);
	if (plan->flags == F_ATLEAST)
		return ((plan->m_data | mode) == mode);
	else if (plan->flags == F_ANY)
		return ((plan->m_data & mode) != 0);
	else
		return (mode == plan->m_data);
	/* NOTREACHED */
}

PLAN *
c_perm(char ***argvp, int isok)
{
	char *perm = **argvp;
	PLAN *new;
	mode_t *set;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_PERM, f_perm);

	if (*perm == '-') {
		new->flags = F_ATLEAST;
		++perm;
	} else if (*perm == '+') {
		new->flags = F_ANY;
		++perm;
	}

	if ((set = setmode(perm)) == NULL)
		err(1, "-perm: Cannot set file mode `%s'", perm);

	new->m_data = getmode(set, 0);
	free(set);
	return (new);
}

/*
 * -print functions --
 *
 *	Always true, causes the current pathame to be written to
 *	standard output.
 */
int
f_print(PLAN *plan, FTSENT *entry)
{

	(void)printf("%s\n", entry->fts_path);
	return (1);
}

int
f_print0(PLAN *plan, FTSENT *entry)
{

	(void)fputs(entry->fts_path, stdout);
	(void)fputc('\0', stdout);
	return (1);
}

int
f_printx(PLAN *plan, FTSENT *entry)
{
	char *cp;

	for (cp = entry->fts_path; *cp; cp++) {
		if (*cp == '\'' || *cp == '\"' || *cp == ' ' ||
		    *cp == '$'  || *cp == '`'  ||
		    *cp == '\t' || *cp == '\n' || *cp == '\\')
			fputc('\\', stdout);

		fputc(*cp, stdout);
	}

	fputc('\n', stdout);
	return (1);
}

PLAN *
c_print(char ***argvp, int isok)
{

	isoutput = 1;

	return (palloc(N_PRINT, f_print));
}

PLAN *
c_print0(char ***argvp, int isok)
{

	isoutput = 1;

	return (palloc(N_PRINT0, f_print0));
}

PLAN *
c_printx(char ***argvp, int isok)
{

	isoutput = 1;

	return (palloc(N_PRINTX, f_printx));
}

/*
 * -prune functions --
 *
 *	Prune a portion of the hierarchy.
 */
int
f_prune(PLAN *plan, FTSENT *entry)
{
	if (fts_set(tree, entry, FTS_SKIP))
		err(1, "%s", entry->fts_path);
	return (1);
}

PLAN *
c_prune(char ***argvp, int isok)
{

	return (palloc(N_PRUNE, f_prune));
}

/*
 * -regex regexp (and related) functions --
 *
 *	True if the complete file path matches the regular expression regexp.
 *	For -regex, regexp is a case-sensitive (basic) regular expression.
 *	For -iregex, regexp is a case-insensitive (basic) regular expression.
 */
int
f_regex(PLAN *plan, FTSENT *entry)
{

	return (regexec(&plan->regexp_data, entry->fts_path, 0, NULL, 0) == 0);
}

static PLAN *
c_regex_common(char ***argvp, int isok, enum ntype type, bool icase)
{
	char errbuf[100];
	regex_t reg;
	char *regexp = **argvp;
	char *lineregexp;
	PLAN *new;
	int rv;
	size_t len;

	(*argvp)++;

	len = strlen(regexp) + 1 + 6;
	lineregexp = malloc(len);	/* max needed */
	if (lineregexp == NULL)
		err(1, NULL);
	snprintf(lineregexp, len, "^%s(%s%s)$",
	    (regcomp_flags & REG_EXTENDED) ? "" : "\\", regexp,
	    (regcomp_flags & REG_EXTENDED) ? "" : "\\");
	rv = regcomp(&reg, lineregexp, REG_NOSUB|regcomp_flags|
	    (icase ? REG_ICASE : 0));
	free(lineregexp);
	if (rv != 0) {
		regerror(rv, &reg, errbuf, sizeof errbuf);
		errx(1, "regexp %s: %s", regexp, errbuf);
	}

	new = palloc(type, f_regex);
	new->regexp_data = reg;
	return (new);
}

PLAN *
c_regex(char ***argvp, int isok)
{

	return (c_regex_common(argvp, isok, N_REGEX, false));
}

PLAN *
c_iregex(char ***argvp, int isok)
{

	return (c_regex_common(argvp, isok, N_IREGEX, true));
}

/*
 * -size n[c] functions --
 *
 *	True if the file size in bytes, divided by an implementation defined
 *	value and rounded up to the next integer, is n.  If n is followed by
 *	a c, the size is in bytes.
 */
#define	FIND_SIZE	512
static int divsize = 1;

int
f_size(PLAN *plan, FTSENT *entry)
{
	off_t size;

	size = divsize ? (entry->fts_statp->st_size + FIND_SIZE - 1) /
	    FIND_SIZE : entry->fts_statp->st_size;
	COMPARE(size, plan->o_data);
}

PLAN *
c_size(char ***argvp, int isok)
{
	char *arg = **argvp;
	PLAN *new;
	char endch;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_SIZE, f_size);
	endch = 'c';
	new->o_data = find_parsenum(new, "-size", arg, &endch);
	if (endch == 'c')
		divsize = 0;
	return (new);
}

/*
 * -type c functions --
 *
 *	True if the type of the file is c, where c is b, c, d, p, f or w
 *	for block special file, character special file, directory, FIFO,
 *	regular file or whiteout respectively.
 */
int
f_type(PLAN *plan, FTSENT *entry)
{

	return ((entry->fts_statp->st_mode & S_IFMT) == plan->m_data);
}

PLAN *
c_type(char ***argvp, int isok)
{
	char *typestring = **argvp;
	PLAN *new;
	mode_t  mask = (mode_t)0;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	switch (typestring[0]) {
	case 'b':
		mask = S_IFBLK;
		break;
	case 'c':
		mask = S_IFCHR;
		break;
	case 'd':
		mask = S_IFDIR;
		break;
	case 'f':
		mask = S_IFREG;
		break;
	case 'l':
		mask = S_IFLNK;
		break;
	case 'p':
		mask = S_IFIFO;
		break;
#ifdef S_IFSOCK
	case 's':
		mask = S_IFSOCK;
		break;
#endif
#ifdef S_IFWHT
	case 'W':
	case 'w':
		mask = S_IFWHT;
#ifdef FTS_WHITEOUT
		ftsoptions |= FTS_WHITEOUT;
#endif
		break;
#endif /* S_IFWHT */
	default:
		errx(1, "-type: %s: unknown type", typestring);
	}

	new = palloc(N_TYPE, f_type);
	new->m_data = mask;
	return (new);
}

/*
 * -user uname functions --
 *
 *	True if the file belongs to the user uname.  If uname is numeric and
 *	an equivalent of the getpwnam() S9.2.2 [POSIX.1] function does not
 *	return a valid user name, uname is taken as a user ID.
 */
int
f_user(PLAN *plan, FTSENT *entry)
{

	COMPARE(entry->fts_statp->st_uid, plan->u_data);
}

PLAN *
c_user(char ***argvp, int isok)
{
	char *username = **argvp;
	PLAN *new;
	struct passwd *p;
	uid_t uid;

	(*argvp)++;
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_USER, f_user);
	p = getpwnam(username);
	if (p == NULL) {
		if (atoi(username) == 0 && username[0] != '0' &&
		    strcmp(username, "+0") && strcmp(username, "-0"))
			errx(1, "-user: %s: no such user", username);
		uid = find_parsenum(new, "-user", username, NULL);

	} else {
		new->flags = F_EQUAL;
		uid = p->pw_uid;
	}

	new->u_data = uid;
	return (new);
}

/*
 * -xdev functions --
 *
 *	Always true, causes find not to descend past directories that have a
 *	different device ID (st_dev, see stat() S5.6.2 [POSIX.1])
 */
PLAN *
c_xdev(char ***argvp, int isok)
{
	ftsoptions |= FTS_XDEV;

	return (palloc(N_XDEV, f_always_true));
}

/*
 * ( expression ) functions --
 *
 *	True if expression is true.
 */
int
f_expr(PLAN *plan, FTSENT *entry)
{
	PLAN *p;
	int state;

	state = 0;
	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (state);
}

/*
 * N_OPENPAREN and N_CLOSEPAREN nodes are temporary place markers.  They are
 * eliminated during phase 2 of find_formplan() --- the '(' node is converted
 * to a N_EXPR node containing the expression and the ')' node is discarded.
 */
PLAN *
c_openparen(char ***argvp, int isok)
{

	return (palloc(N_OPENPAREN, (int (*)(PLAN *, FTSENT *))-1));
}

PLAN *
c_closeparen(char ***argvp, int isok)
{

	return (palloc(N_CLOSEPAREN, (int (*)(PLAN *, FTSENT *))-1));
}

/*
 * ! expression functions --
 *
 *	Negation of a primary; the unary NOT operator.
 */
int
f_not(PLAN *plan, FTSENT *entry)
{
	PLAN *p;
	int state;

	state = 0;
	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (!state);
}

PLAN *
c_not(char ***argvp, int isok)
{

	return (palloc(N_NOT, f_not));
}

/*
 * expression -o expression functions --
 *
 *	Alternation of primaries; the OR operator.  The second expression is
 * not evaluated if the first expression is true.
 */
int
f_or(PLAN *plan, FTSENT *entry)
{
	PLAN *p;
	int state;

	state = 0;
	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);

	if (state)
		return (1);

	for (p = plan->p_data[1];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (state);
}

PLAN *
c_or(char ***argvp, int isok)
{

	return (palloc(N_OR, f_or));
}

PLAN *
c_null(char ***argvp, int isok)
{

	return (NULL);
}


/*
 * plan_cleanup --
 *	Check and see if the specified plan has any residual state,
 *	and if so, clean it up as appropriate.
 *
 *	At the moment, only N_EXEC has state. Two kinds: 1)
 * 	lists of files to feed to subprocesses 2) State on exit
 *	statusses of past subprocesses.
 */
/* ARGSUSED1 */
int
plan_cleanup(PLAN *plan, void *arg)
{
	if (plan->type==N_EXEC && plan->ep_narg)
		run_f_exec(plan);

	return plan->ep_rval;		/* Passed save exit-status up chain */
}

static PLAN *
palloc(enum ntype t, int (*f)(PLAN *, FTSENT *))
{
	PLAN *new;

	if ((new = malloc(sizeof(PLAN))) == NULL)
		err(1, NULL);
	memset(new, 0, sizeof(PLAN));
	new->type = t;
	new->eval = f;
	return (new);
}
