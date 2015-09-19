/*	$NetBSD: find.c,v 1.29 2012/03/20 20:34:57 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)find.c	8.5 (Berkeley) 8/5/94";
#else
__RCSID("$NetBSD: find.c,v 1.29 2012/03/20 20:34:57 matt Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "find.h"

static int ftscompare(const FTSENT **, const FTSENT **);

/*
 * find_formplan --
 *	process the command line and create a "plan" corresponding to the
 *	command arguments.
 */
PLAN *
find_formplan(char **argv)
{
	PLAN *plan, *tail, *new;

	/*
	 * for each argument in the command line, determine what kind of node
	 * it is, create the appropriate node type and add the new plan node
	 * to the end of the existing plan.  The resulting plan is a linked
	 * list of plan nodes.  For example, the string:
	 *
	 *	% find . -name foo -newer bar -print
	 *
	 * results in the plan:
	 *
	 *	[-name foo]--> [-newer bar]--> [-print]
	 *
	 * in this diagram, `[-name foo]' represents the plan node generated
	 * by c_name() with an argument of foo and `-->' represents the
	 * plan->next pointer.
	 */
	for (plan = tail = NULL; *argv;) {
		if (!(new = find_create(&argv)))
			continue;
		if (plan == NULL)
			tail = plan = new;
		else {
			tail->next = new;
			tail = new;
		}
	}

	/*
	 * if the user didn't specify one of -print, -ok, -fprint, -exec, or
	 * -exit, then -print is assumed so we bracket the current expression
	 * with parens, if necessary, and add a -print node on the end.
	 */
	if (!isoutput) {
		if (plan == NULL) {
			new = c_print(NULL, 0);
			tail = plan = new;
		} else {
			new = c_openparen(NULL, 0);
			new->next = plan;
			plan = new;
			new = c_closeparen(NULL, 0);
			tail->next = new;
			tail = new;
			new = c_print(NULL, 0);
			tail->next = new;
			tail = new;
		}
	}

	/*
	 * the command line has been completely processed into a search plan
	 * except for the (, ), !, and -o operators.  Rearrange the plan so
	 * that the portions of the plan which are affected by the operators
	 * are moved into operator nodes themselves.  For example:
	 *
	 *	[!]--> [-name foo]--> [-print]
	 *
	 * becomes
	 *
	 *	[! [-name foo] ]--> [-print]
	 *
	 * and
	 *
	 *	[(]--> [-depth]--> [-name foo]--> [)]--> [-print]
	 *
	 * becomes
	 *
	 *	[expr [-depth]-->[-name foo] ]--> [-print]
	 *
	 * operators are handled in order of precedence.
	 */

	plan = paren_squish(plan);		/* ()'s */
	plan = not_squish(plan);		/* !'s */
	plan = or_squish(plan);			/* -o's */
	return (plan);
}

static int
ftscompare(const FTSENT **e1, const FTSENT **e2)
{

	return (strcoll((*e1)->fts_name, (*e2)->fts_name));
}

static sigset_t ss;
static bool notty;

static __inline void
sig_init(void)
{
	struct sigaction sa;
	notty = !(isatty(STDIN_FILENO) || isatty(STDOUT_FILENO) ||
	    isatty(STDERR_FILENO));
	if (notty)
		return;
	sigemptyset(&ss);
	sigaddset(&ss, SIGINFO); /* block SIGINFO */

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = show_path;
	(void)sigaction(SIGINFO, &sa, NULL);

}

static __inline void
sig_lock(sigset_t *s)
{
	if (notty)
		return;
	sigprocmask(SIG_BLOCK, &ss, s);
}

static __inline void
sig_unlock(const sigset_t *s)
{
	if (notty)
		return;
	sigprocmask(SIG_SETMASK, s, NULL);
}

FTS *tree;			/* pointer to top of FTS hierarchy */
FTSENT *g_entry;		/* shared with SIGINFO handler */

/*
 * find_execute --
 *	take a search plan and an array of search paths and executes the plan
 *	over all FTSENT's returned for the given search paths.
 */
int
find_execute(PLAN *plan, char **paths)
{
	PLAN *p;
	int r, rval, cval;
	sigset_t s;

	cval = 1;

	if (!(tree = fts_open(paths, ftsoptions, issort ? ftscompare : NULL)))
		err(1, "ftsopen");

	sig_init();
	sig_lock(&s);
	for (rval = 0; cval && (g_entry = fts_read(tree)) != NULL;) {
		switch (g_entry->fts_info) {
		case FTS_D:
			if (isdepth)
				continue;
			break;
		case FTS_DP:
			if (!isdepth)
				continue;
			break;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			sig_unlock(&s);
			(void)fflush(stdout);
			warnx("%s: %s",
			    g_entry->fts_path, strerror(g_entry->fts_errno));
			rval = 1;
			sig_lock(&s);
			continue;
		}
#define	BADCH	" \t\n\\'\""
		if (isxargs && strpbrk(g_entry->fts_path, BADCH)) {
			sig_unlock(&s);
			(void)fflush(stdout);
			warnx("%s: illegal path", g_entry->fts_path);
			rval = 1;
			sig_lock(&s);
			continue;
		}

		/*
		 * Call all the functions in the execution plan until one is
		 * false or all have been executed.  This is where we do all
		 * the work specified by the user on the command line.
		 */
		sig_unlock(&s);
		for (p = plan; p && (p->eval)(p, g_entry); p = p->next)
			if (p->type == N_EXIT) {
				rval = p->exit_val;
				cval = 0;
			}
		sig_lock(&s);
	}

	sig_unlock(&s);
	if (g_entry == NULL && errno)
		err(1, "fts_read");
	(void)fts_close(tree);

	/*
	 * Cleanup any plans with leftover state.
	 * Keep the last non-zero return value.
	 */
	if ((r = find_traverse(plan, plan_cleanup, NULL)) != 0)
		rval = r;

	return (rval);
}

/*
 * find_traverse --
 *	traverse the plan tree and execute func() on all plans.  This
 *	does not evaluate each plan's eval() function; it is intended
 *	for operations that must run on all plans, such as state
 *	cleanup.
 *
 *	If any func() returns non-zero, then so will find_traverse().
 */
int
find_traverse(PLAN *plan, int (*func)(PLAN *, void *), void *arg)
{
	PLAN *p;
	int r, rval;

	rval = 0;
	for (p = plan; p; p = p->next) {
		if ((r = func(p, arg)) != 0)
			rval = r;
		if (p->type == N_EXPR || p->type == N_OR) {
			if (p->p_data[0])
				if ((r = find_traverse(p->p_data[0],
					    func, arg)) != 0)
					rval = r;
			if (p->p_data[1])
				if ((r = find_traverse(p->p_data[1],
					    func, arg)) != 0)
					rval = r;
		}
	}
	return rval;
}
