/*	$NetBSD: ex_version.c,v 1.3 2014/01/26 21:43:45 christos Exp $	*/
/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: ex_version.c,v 10.32 2001/06/25 15:19:22 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:22 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: ex_version.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"
#include "version.h"

/*
 * ex_version -- :version
 *	Display the program version.
 *
 * PUBLIC: int ex_version __P((SCR *, EXCMD *));
 */
int
ex_version(SCR *sp, EXCMD *cmdp)
{
	msgq(sp, M_INFO, "Version "VI_VERSION
			 " The CSRG, University of California, Berkeley.");
	return (0);
}
