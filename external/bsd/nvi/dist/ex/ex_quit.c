/*	$NetBSD: ex_quit.c,v 1.3 2014/01/26 21:43:45 christos Exp $	*/
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
static const char sccsid[] = "Id: ex_quit.c,v 10.8 2001/06/25 15:19:18 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:18 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: ex_quit.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_quit -- :quit[!]
 *	Quit.
 *
 * PUBLIC: int ex_quit __P((SCR *, EXCMD *));
 */
int
ex_quit(SCR *sp, EXCMD *cmdp)
{
	int force;

	force = FL_ISSET(cmdp->iflags, E_C_FORCE);

	/* Check for file modifications, or more files to edit. */
	if (file_m2(sp, force) || ex_ncheck(sp, force))
		return (1);

	F_SET(sp, force ? SC_EXIT_FORCE : SC_EXIT);
	return (0);
}
