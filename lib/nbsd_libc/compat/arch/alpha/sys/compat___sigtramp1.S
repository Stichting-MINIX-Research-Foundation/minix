/*	$NetBSD: compat___sigtramp1.S,v 1.1 2005/09/16 18:21:19 drochner Exp $	*/

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
 *	sp->	sigcontext structure
 */
NESTED_NOPROFILE(__sigtramp_sigcontext_1,0,0,ra,0,0)
	mov	sp, a0			/* get pointer to sigcontext */
	CALLSYS_NOERROR(compat_16___sigreturn14)	/* and call sigreturn() with it */
	mov	v0, a0			/* if that failed, get error code */
	CALLSYS_NOERROR(exit)		/* and call exit() with it */
END(__sigtramp_sigcontext_1)
