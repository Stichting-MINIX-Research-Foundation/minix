/*	$NetBSD: ex_display.c,v 1.3 2013/11/25 22:43:46 christos Exp $ */
/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: ex_display.c,v 10.15 2001/06/25 15:19:15 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:15 ";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../common/common.h"
#include "tag.h"

static int	is_prefix __P((ARGS *, const CHAR_T *));
static int	bdisplay __P((SCR *));
static void	db __P((SCR *, CB *, const char *));

/*
 * ex_display -- :display b[uffers] | c[onnections] | s[creens] | t[ags]
 *
 *	Display cscope connections, buffers, tags or screens.
 *
 * PUBLIC: int ex_display __P((SCR *, EXCMD *));
 */
int
ex_display(SCR *sp, EXCMD *cmdp)
{
	ARGS *arg;

	arg = cmdp->argv[0];

	switch (arg->bp[0]) {
	case L('b'):
		if (!is_prefix(arg, L("buffers")))
			break;
		return (bdisplay(sp));
	case L('c'):
		if (!is_prefix(arg, L("connections")))
			break;
		return (cscope_display(sp));
	case L('s'):
		if (!is_prefix(arg, L("screens")))
			break;
		return (ex_sdisplay(sp));
	case L('t'):
		if (!is_prefix(arg, L("tags")))
			break;
		return (ex_tag_display(sp));
	}
	ex_emsg(sp, cmdp->cmd->usage, EXM_USAGE);
	return (1);
}

/*
 * is_prefix --
 *
 *	Check that a command argument matches a prefix of a given string.
 */
static int
is_prefix(ARGS *arg, const CHAR_T *str)
{
	return arg->len <= STRLEN(str) && !MEMCMP(arg->bp, str, arg->len);
}

/*
 * bdisplay --
 *
 *	Display buffers.
 */
static int
bdisplay(SCR *sp)
{
	CB *cbp;

	if (LIST_EMPTY(&sp->wp->cutq) && sp->wp->dcbp == NULL) {
		msgq(sp, M_INFO, "123|No cut buffers to display");
		return (0);
	}

	/* Display regular cut buffers. */
	LIST_FOREACH(cbp, &sp->wp->cutq, q) {
		if (ISDIGIT(cbp->name))
			continue;
		if (!TAILQ_EMPTY(&cbp->textq))
			db(sp, cbp, NULL);
		if (INTERRUPTED(sp))
			return (0);
	}
	/* Display numbered buffers. */
	LIST_FOREACH(cbp, &sp->wp->cutq, q) {
		if (!ISDIGIT(cbp->name))
			continue;
		if (!TAILQ_EMPTY(&cbp->textq))
			db(sp, cbp, NULL);
		if (INTERRUPTED(sp))
			return (0);
	}
	/* Display default buffer. */
	if ((cbp = sp->wp->dcbp) != NULL)
		db(sp, cbp, "default buffer");
	return (0);
}

/*
 * db --
 *	Display a buffer.
 */
static void
db(SCR *sp, CB *cbp, const char *np)
{
	CHAR_T *p;
	TEXT *tp;
	size_t len;
	const unsigned char *name = (const void *)np;

	(void)ex_printf(sp, "********** %s%s\n",
	    name == NULL ? KEY_NAME(sp, cbp->name) : name,
	    F_ISSET(cbp, CB_LMODE) ? " (line mode)" : " (character mode)");
	TAILQ_FOREACH(tp, &cbp->textq, q) {
		for (len = tp->len, p = tp->lb; len--; ++p) {
			(void)ex_puts(sp, (char *)KEY_NAME(sp, *p));
			if (INTERRUPTED(sp))
				return;
		}
		(void)ex_puts(sp, "\n");
	}
}
