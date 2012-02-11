/*	$NetBSD: psignal.c,v 1.22 2010/08/27 08:38:41 christos Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
static char sccsid[] = "@(#)psignal.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: psignal.c,v 1.22 2010/08/27 08:38:41 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <sys/uio.h>

#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#ifdef __weak_alias
__weak_alias(psignal,_psignal)
#endif

void
psignal(int sig, const char *s)
{
	struct iovec *v;
	struct iovec iov[4];
	char buf[NL_TEXTMAX];

	v = iov;
	if (s && *s) {
		v->iov_base = __UNCONST(s);
		v->iov_len = strlen(s);
		v++;
		v->iov_base = __UNCONST(": ");
		v->iov_len = 2;
		v++;
	}
	v->iov_base = __UNCONST(__strsignal((int)sig, buf, sizeof(buf)));
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = __UNCONST("\n");
	v->iov_len = 1;
	(void)writev(STDERR_FILENO, iov, (v - iov) + 1);
}

void
psiginfo(const siginfo_t *si, const char *s)
{
	psignal(si->si_signo, s);
}
