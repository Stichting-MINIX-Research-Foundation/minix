/*	$NetBSD: signal.h,v 1.2 2009/01/11 02:46:25 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)signal.h	8.3 (Berkeley) 3/30/94
 */

#ifndef _COMPAT_SIGNAL_H_
#define _COMPAT_SIGNAL_H_

#include <compat/sys/signal.h>

__BEGIN_DECLS
int	sigaction(int, const struct sigaction13 * __restrict,
    struct sigaction13 * __restrict);
int	__sigaction14(int, const struct sigaction * __restrict,
    struct sigaction * __restrict);
int	sigaddset(sigset13_t *, int);
int	__sigaddset14(sigset_t *, int);
int	sigdelset(sigset13_t *, int);
int	__sigdelset14(sigset_t *, int);
int	sigemptyset(sigset13_t *);
int	__sigemptyset14(sigset_t *);
int	sigfillset(sigset13_t *);
int	__sigfillset14(sigset_t *);
int	sigismember(const sigset13_t *, int);
int	__sigismember14(const sigset_t *, int);
int	sigpending(sigset13_t *);
int	__sigpending14(sigset_t *);
int	sigprocmask(int, const sigset13_t * __restrict,
    sigset13_t * __restrict);
int	__sigprocmask14(int, const sigset_t * __restrict,
    sigset_t * __restrict);
int	sigsuspend(const sigset13_t *);
int	__sigsuspend14(const sigset_t *);

int	sigtimedwait(const sigset_t * __restrict,
    siginfo_t * __restrict, const struct timespec50 * __restrict);
int	__sigtimedwait(const sigset_t * __restrict,
    siginfo_t * __restrict, struct timespec50 * __restrict);
int	__sigtimedwait50(const sigset_t * __restrict,
    siginfo_t * __restrict, const struct timespec * __restrict);
int	____sigtimedwait50(const sigset_t * __restrict,
    siginfo_t * __restrict, struct timespec * __restrict);
/*
 * X/Open CAE Specification Issue 4 Version 2
 */      
#if (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
int	sigaltstack(const struct sigaltstack13 * __restrict,
    struct sigaltstack13 * __restrict);
int	__sigaltstack14(const stack_t * __restrict, stack_t * __restrict);
#endif /* _XOPEN_SOURCE_EXTENDED || _XOPEN_SOURCE >= 500 || _NETBSD_SOURCE */

__END_DECLS

#endif	/* !_COMPAT_SIGNAL_H_ */
