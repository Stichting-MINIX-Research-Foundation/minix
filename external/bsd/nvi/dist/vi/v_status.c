/*	$NetBSD: v_status.c,v 1.3 2014/01/26 21:43:45 christos Exp $	*/
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
static const char sccsid[] = "Id: v_status.c,v 10.10 2001/06/25 15:19:35 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:35 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: v_status.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"
#include "vi.h"

/*
 * v_status -- ^G
 *	Show the file status.
 *
 * PUBLIC: int v_status __P((SCR *, VICMD *));
 */
int
v_status(SCR *sp, VICMD *vp)
{
	(void)msgq_status(sp, vp->m_start.lno, MSTAT_SHOWLAST);
	return (0);
}
