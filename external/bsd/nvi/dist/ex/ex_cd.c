/*	$NetBSD: ex_cd.c,v 1.3 2014/01/26 21:43:45 christos Exp $ */
/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: ex_cd.c,v 10.12 2001/06/25 15:19:14 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:14 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: ex_cd.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/param.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

/*
 * ex_cd -- :cd[!] [directory]
 *	Change directories.
 *
 * PUBLIC: int ex_cd __P((SCR *, EXCMD *));
 */
int
ex_cd(SCR *sp, EXCMD *cmdp)
{
	struct passwd *pw;
	ARGS *ap;
	const char *p, *t;	/* XXX: END OF THE STACK, DON'T TRUST GETCWD. */
	const char *dir;
	char buf[MAXPATHLEN * 2];
	size_t dlen;

	/*
	 * !!!
	 * Historic practice is that the cd isn't attempted if the file has
	 * been modified, unless its name begins with a leading '/' or the
	 * force flag is set.
	 */
	if (F_ISSET(sp->ep, F_MODIFIED) &&
	    !FL_ISSET(cmdp->iflags, E_C_FORCE) && sp->frp->name[0] != '/') {
		msgq(sp, M_ERR,
    "120|File modified since last complete write; write or use ! to override");
		return (1);
	}

	switch (cmdp->argc) {
	case 0:
		/* If no argument, change to the user's home directory. */
		if ((dir = getenv("HOME")) == NULL) {
			if ((pw = getpwuid(getuid())) == NULL ||
			    pw->pw_dir == NULL || pw->pw_dir[0] == '\0') {
				msgq(sp, M_ERR,
			   "121|Unable to find home directory location");
				return (1);
			}
			dir = pw->pw_dir;
		}
		break;
	case 1:
		INT2CHAR(sp, cmdp->argv[0]->bp, cmdp->argv[0]->len + 1, 
			 dir, dlen);
		break;
	default:
		abort();
	}

	/*
	 * Try the current directory first.  If this succeeds, don't display
	 * a message, vi didn't historically, and it should be obvious to the
	 * user where they are.
	 */
	if (!chdir(dir))
		return (0);

	/*
	 * If moving to the user's home directory, or, the path begins with
	 * "/", "./" or "../", it's the only place we try.
	 */
	if (cmdp->argc == 0 ||
	    (ap = cmdp->argv[0])->bp[0] == '/' ||
	    (ap->len == 1 && ap->bp[0] == '.') ||
	    (ap->len >= 2 && ap->bp[0] == '.' && ap->bp[1] == '.' &&
	    (ap->bp[2] == '/' || ap->bp[2] == '\0')))
		goto err;

	/* Try the O_CDPATH option values. */
	for (p = t = O_STR(sp, O_CDPATH);; ++p)
		if (*p == '\0' || *p == ':') {
			/*
			 * Empty strings specify ".".  The only way to get an
			 * empty string is a leading colon, colons in a row,
			 * or a trailing colon.  Or, to put it the other way,
			 * if the length is 1 or less, then we're dealing with
			 * ":XXX", "XXX::XXXX" , "XXX:", or "".  Since we've
			 * already tried dot, we ignore tham all.
			 */
			if (t < p - 1) {
				(void)snprintf(buf,
				    sizeof(buf), "%.*s/%s", (int)(p - t),
					t, dir);
				if (!chdir(buf)) {
					if (getcwd(buf, sizeof(buf)) != NULL)
		msgq_str(sp, M_INFO, buf, "122|New current directory: %s");
					return (0);
				}
			}
			t = p + 1;
			if (*p == '\0')
				break;
		}

err:	msgq_str(sp, M_SYSERR, dir, "%s");
	return (1);
}
