/*	$NetBSD: v_redraw.c,v 1.3 2014/01/26 21:43:45 christos Exp $	*/
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
static const char sccsid[] = "Id: v_redraw.c,v 10.7 2001/06/25 15:19:34 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:34 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: v_redraw.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"
#include "vi.h"

/*
 * v_redraw -- ^L, ^R
 *	Redraw the screen.
 *
 * PUBLIC: int v_redraw __P((SCR *, VICMD *));
 */
int
v_redraw(SCR *sp, VICMD *vp)
{
	return (sp->gp->scr_refresh(sp, 1));
}
