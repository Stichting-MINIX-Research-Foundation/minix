/*	$NetBSD: ex_tcl.c,v 1.3 2014/01/26 21:43:45 christos Exp $ */
/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: ex_tcl.c,v 8.11 2001/06/25 15:19:21 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:21 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: ex_tcl.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"

#ifdef HAVE_TCL_INTERP
#include <tcl.h>
#endif

/* 
 * ex_tcl -- :[line [,line]] tcl [command]
 *	Run a command through the tcl interpreter.
 *
 * PUBLIC: int ex_tcl __P((SCR*, EXCMD *));
 */
int 
ex_tcl(SCR *sp, EXCMD *cmdp)
{
#ifdef HAVE_TCL_INTERP
	CHAR_T *p;
	GS *gp;
	size_t len;
	char buf[128];

	/* Initialize the interpreter. */
	gp = sp->gp;
	if (gp->tcl_interp == NULL && tcl_init(gp))
		return (1);

	/* Skip leading white space. */
	if (cmdp->argc != 0)
		for (p = cmdp->argv[0]->bp,
		    len = cmdp->argv[0]->len; len > 0; --len, ++p)
			if (!ISBLANK((UCHAR_T)*p))
				break;
	if (cmdp->argc == 0 || len == 0) {
		ex_emsg(sp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}

	(void)snprintf(buf, sizeof(buf),
	    "set viScreenId %d\nset viStartLine %lu\nset viStopLine %lu",
	    sp->id, cmdp->addr1.lno, cmdp->addr2.lno);
	if (Tcl_Eval(gp->tcl_interp, buf) == TCL_OK &&
	    Tcl_Eval(gp->tcl_interp, cmdp->argv[0]->bp) == TCL_OK)
		return (0);

	msgq(sp, M_ERR, "Tcl: %s", ((Tcl_Interp *)gp->tcl_interp)->result);
	return (1);
#else
	msgq(sp, M_ERR, "302|Vi was not loaded with a Tcl interpreter");
	return (1);
#endif /* HAVE_TCL_INTERP */
}
