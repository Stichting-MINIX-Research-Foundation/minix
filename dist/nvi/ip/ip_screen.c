/*	$NetBSD: ip_screen.c,v 1.1.1.2 2008/05/18 14:31:24 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: ip_screen.c,v 8.8 2001/06/25 15:19:24 skimo Exp (Berkeley) Date: 2001/06/25 15:19:24";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>

#include "../common/common.h"
#include "../ipc/ip.h"
#include "extern.h"

/*
 * ip_screen --
 *	Initialize/shutdown the IP screen.
 *
 * PUBLIC: int ip_screen __P((SCR *, u_int32_t));
 */
int
ip_screen(SCR *sp, u_int32_t flags)
{
	GS *gp;
	IP_PRIVATE *ipp;

	gp = sp->gp;
	ipp = IPP(sp);

	/* See if the current information is incorrect. */
	if (F_ISSET(gp, G_SRESTART)) {
		if (ip_quit(sp->wp))
			return (1);
		F_CLR(gp, G_SRESTART);
	}
	
	/* See if we're already in the right mode. */
	if (LF_ISSET(SC_EX) && F_ISSET(sp, SC_SCR_EX) ||
	    LF_ISSET(SC_VI) && F_ISSET(sp, SC_SCR_VI))
		return (0);

	/* Ex isn't possible if there is no terminal. */
	if (LF_ISSET(SC_EX) && ipp->t_fd == -1)
		return (1);

	if (F_ISSET(sp, SC_SCR_EX))
		F_CLR(sp, SC_SCR_EX);

	if (F_ISSET(sp, SC_SCR_VI))
		F_CLR(sp, SC_SCR_VI);

	if (LF_ISSET(SC_EX)) {
		F_SET(ipp, IP_IN_EX);
	} else {
		/* Initialize terminal based information. */
		if (ip_term_init(sp)) 
			return (1);

		F_CLR(ipp, IP_IN_EX);
		F_SET(ipp, IP_SCR_VI_INIT);
	}
	return (0);
}

/*
 * ip_quit --
 *	Shutdown the screens.
 *
 * PUBLIC: int ip_quit __P((WIN *));
 */
int
ip_quit(WIN *wp)
{
	IP_PRIVATE *ipp;
	int rval;

	/* Clean up the terminal mappings. */
	rval = ip_term_end(wp->gp);

	ipp = WIPP(wp);
	F_CLR(ipp, IP_SCR_VI_INIT);

	return (rval);
}
