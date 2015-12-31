/*	$NetBSD: ex_set.c,v 1.3 2014/01/26 21:43:45 christos Exp $	*/
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
static const char sccsid[] = "Id: ex_set.c,v 10.8 2001/06/25 15:19:19 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:19 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: ex_set.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_set -- :set
 *	Ex set option.
 *
 * PUBLIC: int ex_set __P((SCR *, EXCMD *));
 */
int
ex_set(SCR *sp, EXCMD *cmdp)
{
	switch(cmdp->argc) {
	case 0:
		opts_dump(sp, CHANGED_DISPLAY);
		break;
	default:
		if (opts_set(sp, cmdp->argv, cmdp->cmd->usage))
			return (1);
		break;
	}
	return (0);
}
