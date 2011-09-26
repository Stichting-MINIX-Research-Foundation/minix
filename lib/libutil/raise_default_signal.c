/*	$NetBSD: raise_default_signal.c,v 1.3 2008/04/28 20:23:03 martin Exp $	 */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: raise_default_signal.c,v 1.3 2008/04/28 20:23:03 martin Exp $");
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <util.h>

#if ! HAVE_RAISE_DEFAULT_SIGNAL
/*
 * raise_default_signal sig
 *	Raise the default signal handler for sig, by
 *	- block all signals
 *	- set the signal handler to SIG_DFL
 *	- raise the signal
 *	- unblock the signal to deliver it
 *
 *	The original signal mask and signal handler is restored on exit
 *	(whether successful or not).
 *
 *	Returns 0 on success, or -1 on failure with errno set to
 *	on of the values for sigemptyset(), sigaddset(), sigprocmask(),
 *	sigaction(), or raise().
 */
int
raise_default_signal(int sig)
{
	struct sigaction origact, act;
	sigset_t origmask, fullmask, mask;
	int retval, oerrno;

	retval = -1;

		/* Setup data structures */
		/* XXX memset(3) isn't async-safe according to signal(7) */
	(void)memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_DFL;
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) ||
	    (sigfillset(&fullmask) == -1) ||
	    (sigemptyset(&mask) == -1) ||
	    (sigaddset(&mask, sig) == -1))
		goto restore_none;

		/* Block all signals */
	if (sigprocmask(SIG_BLOCK, &fullmask, &origmask) == -1)
		goto restore_none;
		/* (use 'goto restore_mask' to restore state) */

		/* Enable the SIG_DFL handler */
	if (sigaction(sig, &act, &origact) == -1)
		goto restore_mask;
		/* (use 'goto restore_act' to restore state) */

		/* Raise the signal, and unblock the signal to deliver it */
	if ((raise(sig) == -1) ||
	    (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1))
		goto restore_act;

		/* Flag successful raise() */
	retval = 0;

		/* Restore the original handler */
 restore_act:
	oerrno = errno;
	(void)sigaction(sig, &origact, NULL);
	errno = oerrno;

		/* Restore the original mask */
 restore_mask:
	oerrno = errno;
	(void)sigprocmask(SIG_SETMASK, &origmask, NULL);
	errno = oerrno;

 restore_none:
	return retval;
}

#endif	/* ! HAVE_RAISE_DEFAULT_SIGNAL */
