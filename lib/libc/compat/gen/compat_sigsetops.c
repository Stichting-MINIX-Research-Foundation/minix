/*	$NetBSD: compat_sigsetops.c,v 1.3 2012/03/20 17:05:59 matt Exp $	*/

/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)sigsetops.c	8.1 (Berkeley) 6/4/93
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)sigsetops.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: compat_sigsetops.c,v 1.3 2012/03/20 17:05:59 matt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#define	__LIBC12_SOURCE__

#include <errno.h>
#include <sys/time.h>
#include <compat/sys/time.h>
#include <signal.h>
#include <compat/include/signal.h>

#undef sigemptyset
#undef sigfillset
#undef sigaddset
#undef sigdelset
#undef sigismember

__warn_references(sigaddset,
    "warning: reference to compatibility sigaddset(); include <signal.h> for correct reference")
__warn_references(sigdelset,
    "warning: reference to compatibility sigdelset(); include <signal.h> for correct reference")
__warn_references(sigemptyset,
    "warning: reference to compatibility sigemptyset(); include <signal.h> for correct reference")
__warn_references(sigfillset,
    "warning: reference to compatibility sigfillset(); include <signal.h> for correct reference")
__warn_references(sigismember,
    "warning: reference to compatibility sigismember(); include <signal.h> for correct reference")

int
sigemptyset(sigset13_t *set)
{
	*set = 0;
	return (0);
}

int
sigfillset(sigset13_t *set)
{
	*set = ~(sigset13_t)0;
	return (0);
}

int
sigaddset(sigset13_t *set, int signo)
{
	if (signo <= 0 || signo >= NSIG13) {
		errno = EINVAL;
		return -1;
	}
	*set |= __sigmask13(signo);
	return (0);
}

int
sigdelset(sigset13_t *set, int signo)
{
	if (signo <= 0 || signo >= NSIG13) {
		errno = EINVAL;
		return -1;
	}
	*set &= ~__sigmask13(signo);
	return (0);
}

int
sigismember(const sigset13_t *set, int signo)
{
	if (signo <= 0 || signo >= NSIG13) {
		errno = EINVAL;
		return -1;
	}
	return ((*set & __sigmask13(signo)) != 0);
}
