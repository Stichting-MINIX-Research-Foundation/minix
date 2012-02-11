/*	$NetBSD: __sigaction14_sigtramp.c,v 1.7 2008/04/28 20:22:57 martin Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: __sigaction14_sigtramp.c,v 1.7 2008/04/28 20:22:57 martin Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "extern.h"

__weak_alias(__sigaction14, __libc_sigaction14)

int
__libc_sigaction14(int sig, const struct sigaction *act, struct sigaction *oact)
{
	extern void __sigtramp_siginfo_2(void);

#ifdef __LIBC12_SOURCE__
	/*
	 * We select the compatibility SIGCONTEXT trampoline if SA_SIGINFO
	 * is not set in the sigaction.
	 */
	if (act && (act->sa_flags & SA_SIGINFO) == 0) {
		extern void __sigtramp_sigcontext_1(void);
		int sav = errno;
		int rv =  __sigaction_sigtramp(sig, act, oact,
					   __sigtramp_sigcontext_1, 1);
		/*
		 * EINVAL might indicate that trampoline version 1 is
		 * not supported by the kernel; fall back on native
		 * SIGINFO trampoline.
		 */
		if (rv >= 0 || errno != EINVAL)
			return rv;
		errno = sav;
	}
#endif

	return __sigaction_sigtramp(sig, act, oact, __sigtramp_siginfo_2, 2);
}
