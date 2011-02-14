/*	$NetBSD: __sigtramp2.S,v 1.2 2006/11/08 21:16:40 drochner Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "SYS.h"

/*
 * The Alpha signal trampoline is invoked only to return from
 * the signal; the kernel calls the signal handler directly.
 *
 * On entry, the stack looks like:
 *
 *		ucontext structure	[128] == sp + sizeof(siginfo_t)]
 *	sp->	siginfo structure	[0]
 */
NESTED_NOPROFILE(__sigtramp_siginfo_2,0,0,ra,0,0)
	ldgp	gp,0(ra)
	lda	a0,(128)(sp)		/* get pointer to ucontext */
	CALLSYS_NOERROR(setcontext)	/* and call setcontext() with it */
	ldiq	a0,-1			/* if that failed, set an exit code */
	CALLSYS_NOERROR(exit)		/* and call exit() */
END(__sigtramp_siginfo_2)
