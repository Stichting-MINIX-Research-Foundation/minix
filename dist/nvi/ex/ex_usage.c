/*	$NetBSD: ex_usage.c,v 1.6 2011/03/21 14:53:03 tnozaki Exp $ */

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
static const char sccsid[] = "Id: ex_usage.c,v 10.15 2001/06/25 15:19:21 skimo Exp (Berkeley) Date: 2001/06/25 15:19:21";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../vi/vi.h"

/*
 * ex_help -- :help
 *	Display help message.
 *
 * PUBLIC: int ex_help __P((SCR *, EXCMD *));
 */
int
ex_help(SCR *sp, EXCMD *cmdp)
{
	(void)ex_puts(sp,
	    "To see the list of vi commands, enter \":viusage<CR>\"\n");
	(void)ex_puts(sp,
	    "To see the list of ex commands, enter \":exusage<CR>\"\n");
	(void)ex_puts(sp,
	    "For an ex command usage statement enter \":exusage [cmd]<CR>\"\n");
	(void)ex_puts(sp,
	    "For a vi key usage statement enter \":viusage [key]<CR>\"\n");
	(void)ex_puts(sp, "To exit, enter \":q!\"\n");
	return (0);
}

/*
 * ex_usage -- :exusage [cmd]
 *	Display ex usage strings.
 *
 * PUBLIC: int ex_usage __P((SCR *, EXCMD *));
 */
int
ex_usage(SCR *sp, EXCMD *cmdp)
{
	ARGS *ap;
	EXCMDLIST const *cp;
	int newscreen;
	CHAR_T *p, nb[MAXCMDNAMELEN + 5];
	const CHAR_T *name;

	switch (cmdp->argc) {
	case 1:
		ap = cmdp->argv[0];
		if (ISUPPER((UCHAR_T)ap->bp[0])) {
			newscreen = 1;
			ap->bp[0] = TOLOWER((UCHAR_T)ap->bp[0]);
		} else
			newscreen = 0;
		for (cp = cmds; cp->name != NULL &&
		    memcmp(ap->bp, cp->name, ap->len); ++cp);
		if (cp->name == NULL ||
		    (newscreen && !F_ISSET(cp, E_NEWSCREEN))) {
			const char *nstr;
			size_t nlen;

			if (newscreen)
				ap->bp[0] = TOUPPER((UCHAR_T)ap->bp[0]);

			INT2CHAR(sp, ap->bp, ap->len + 1, nstr, nlen);
			(void)ex_printf(sp, "The %.*s command is unknown\n",
			    (int)ap->len, nstr);
		} else {
			(void)ex_printf(sp,
			    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
			/*
			 * !!!
			 * The "visual" command has two modes, one from ex,
			 * one from the vi colon line.  Don't ask.
			 */
			if (cp != &cmds[C_VISUAL_EX] &&
			    cp != &cmds[C_VISUAL_VI])
				break;
			if (cp == &cmds[C_VISUAL_EX])
				cp = &cmds[C_VISUAL_VI];
			else
				cp = &cmds[C_VISUAL_EX];
			(void)ex_printf(sp,
			    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
		}
		break;
	case 0:
		for (cp = cmds; cp->name != NULL && !INTERRUPTED(sp); ++cp) {
			/*
			 * The ^D command has an unprintable name.
			 *
			 * XXX
			 * We display both capital and lower-case versions of
			 * the appropriate commands -- no need to add in extra
			 * room, they're all short names.
			 */
			if (cp == &cmds[C_SCROLL])
				name = L("^D");
			else if (F_ISSET(cp, E_NEWSCREEN)) {
				nb[0] = L('[');
				nb[1] = TOUPPER((UCHAR_T)cp->name[0]);
				nb[2] = cp->name[0];
				nb[3] = L(']');
				for (name = cp->name + 1,
				    p = nb + 4; (*p++ = *name++) != '\0';);
				name = nb;
			} else
				name = cp->name;
			(void)ex_printf(sp,
			    WVS": %s\n", MAXCMDNAMELEN, name, cp->help);
		}
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * ex_viusage -- :viusage [key]
 *	Display vi usage strings.
 *
 * PUBLIC: int ex_viusage __P((SCR *, EXCMD *));
 */
int
ex_viusage(SCR *sp, EXCMD *cmdp)
{
	GS *gp;
	VIKEYS const *kp;
	int key;

	gp = sp->gp;
	switch (cmdp->argc) {
	case 1:
		if (cmdp->argv[0]->len != 1) {
			ex_emsg(sp, cmdp->cmd->usage, EXM_USAGE);
			return (1);
		}
		key = cmdp->argv[0]->bp[0];
		if (key > MAXVIKEY)
			goto nokey;

		/* Special case: '[' and ']' commands. */
		if ((key == '[' || key == ']') && cmdp->argv[0]->bp[1] != key)
			goto nokey;

		/* Special case: ~ command. */
		if (key == '~' && O_ISSET(sp, O_TILDEOP))
			kp = &tmotion;
		else
			kp = &vikeys[key];

		if (kp->usage == NULL)
nokey:			(void)ex_printf(sp,
			    "The %s key has no current meaning\n",
			    KEY_NAME(sp, key));
		else
			(void)ex_printf(sp,
			    "  Key:%s%s\nUsage: %s\n",
			    isblank((unsigned char)*kp->help) ? "" : " ", kp->help, kp->usage);
		break;
	case 0:
		for (key = 0; key <= MAXVIKEY && !INTERRUPTED(sp); ++key) {
			/* Special case: ~ command. */
			if (key == '~' && O_ISSET(sp, O_TILDEOP))
				kp = &tmotion;
			else
				kp = &vikeys[key];
			if (kp->help != NULL)
				(void)ex_printf(sp, "%s\n", kp->help);
		}
		break;
	default:
		abort();
	}
	return (0);
}
