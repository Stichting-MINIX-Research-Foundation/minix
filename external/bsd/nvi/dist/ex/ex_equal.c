/*	$NetBSD: ex_equal.c,v 1.3 2014/01/26 21:43:45 christos Exp $ */
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
static const char sccsid[] = "Id: ex_equal.c,v 10.12 2001/06/25 15:19:15 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:15 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: ex_equal.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_equal -- :address =
 *
 * PUBLIC: int ex_equal __P((SCR *, EXCMD *));
 */
int
ex_equal(SCR *sp, EXCMD *cmdp)
{
	db_recno_t lno;

	NEEDFILE(sp, cmdp);

	/*
	 * Print out the line number matching the specified address,
	 * or the number of the last line in the file if no address
	 * specified.
	 *
	 * !!!
	 * Historically, ":0=" displayed 0, and ":=" or ":1=" in an
	 * empty file displayed 1.  Until somebody complains loudly,
	 * we're going to do it right.  The tables in excmd.c permit
	 * lno to get away with any address from 0 to the end of the
	 * file, which, in an empty file, is 0.
	 */
	if (F_ISSET(cmdp, E_ADDR_DEF)) {
		if (db_last(sp, &lno))
			return (1);
	} else
		lno = cmdp->addr1.lno;

	(void)ex_printf(sp, "%ld\n", (unsigned long)lno);
	return (0);
}
