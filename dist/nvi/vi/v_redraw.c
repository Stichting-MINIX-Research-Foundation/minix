/*	$NetBSD: v_redraw.c,v 1.1.1.2 2008/05/18 14:31:43 aymeric Exp $ */

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
static const char sccsid[] = "Id: v_redraw.c,v 10.7 2001/06/25 15:19:34 skimo Exp (Berkeley) Date: 2001/06/25 15:19:34";
#endif /* not lint */

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
