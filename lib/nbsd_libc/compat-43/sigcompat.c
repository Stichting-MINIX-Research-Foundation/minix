/*	$NetBSD: sigcompat.c,v 1.13 2005/12/24 21:11:16 perry Exp $	*/

/*
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)sigcompat.c	8.1 (Berkeley) 6/2/93";
#else
__RCSID("$NetBSD: sigcompat.c,v 1.13 2005/12/24 21:11:16 perry Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <signal.h>
#include <compat/sys/signal.h>

static inline void sv2sa(struct sigaction *, const struct sigvec *);
static inline void sa2sv(struct sigvec *, const struct sigaction *);

static inline void
sv2sa(struct sigaction *sa, const struct sigvec *sv)
{
	sigemptyset(&sa->sa_mask);
	sa->sa_mask.__bits[0] = sv->sv_mask;
	sa->sa_handler = sv->sv_handler;
	sa->sa_flags = sv->sv_flags ^ SV_INTERRUPT; /* !SA_INTERRUPT */
}

static inline void
sa2sv(struct sigvec *sv, const struct sigaction *sa)
{
	sv->sv_mask = sa->sa_mask.__bits[0];
	sv->sv_handler = sa->sa_handler;
	sv->sv_flags = sa->sa_flags ^ SV_INTERRUPT; /* !SA_INTERRUPT */
}
	
int
sigvec(int signo, struct sigvec *nsv, struct sigvec *osv)
{
	int ret;
	struct sigaction osa, nsa;

	if (nsv)
		sv2sa(&nsa, nsv);

	ret = sigaction(signo, nsv ? &nsa : NULL, osv ? &osa : NULL);

	if (ret == 0 && osv)
		sa2sv(osv, &osa);

	return (ret);
}

int
sigsetmask(int mask)
{
	sigset_t nmask, omask;
	int n;

	sigemptyset(&nmask);
	nmask.__bits[0] = mask;

	n = sigprocmask(SIG_SETMASK, &nmask, &omask);
	if (n)
		return (n);
	return (omask.__bits[0]);
}

int
sigblock(int mask)
{
	sigset_t nmask, omask;
	int n;

	sigemptyset(&nmask);
	nmask.__bits[0] = mask;

	n = sigprocmask(SIG_BLOCK, &nmask, &omask);
	if (n)
		return (n);
	return (omask.__bits[0]);
}

int
sigpause(int mask)
{
	sigset_t nmask;

	sigemptyset(&nmask);
	nmask.__bits[0] = mask;
	return (sigsuspend(&nmask));
}
