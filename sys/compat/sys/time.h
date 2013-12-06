/*	$NetBSD: time.h,v 1.5 2013/10/04 21:07:37 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)time.h	8.5 (Berkeley) 5/4/95
 */

#ifndef _COMPAT_SYS_TIME_H_
#define	_COMPAT_SYS_TIME_H_

#include <sys/featuretest.h>
#include <sys/types.h>

#include <compat/sys/time_types.h>

#if !defined(_KERNEL) && !defined(_STANDALONE)
__BEGIN_DECLS
#if (_POSIX_C_SOURCE - 0) >= 200112L || \
    defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
int	getitimer(int, struct itimerval50 *);
int	__compat_gettimeofday(struct timeval50 * __restrict, void *__restrict)
    __dso_hidden;
int	setitimer(int, const struct itimerval50 * __restrict,
	    struct itimerval50 * __restrict);
int	utimes(const char *, const struct timeval50 [2]);
int	__getitimer50(int, struct itimerval *);
int	__gettimeofday50(struct timeval * __restrict, void *__restrict);
int	__setitimer50(int, const struct itimerval * __restrict,
	    struct itimerval * __restrict);
int	__utimes50(const char *, const struct timeval [2]);
#endif /* _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE || _NETBSD_SOURCE */

#if defined(_NETBSD_SOURCE)
int	adjtime(const struct timeval50 *, struct timeval50 *);
int	futimes(int, const struct timeval50 [2]);
int	lutimes(const char *, const struct timeval50 [2]);
int	settimeofday(const struct timeval50 * __restrict,
	    const void *__restrict);
int	__adjtime50(const struct timeval *, struct timeval *);
int	__futimes50(int, const struct timeval [2]);
int	__lutimes50(const char *, const struct timeval [2]);
int	__settimeofday50(const struct timeval * __restrict,
	    const void *__restrict);
#endif /* _NETBSD_SOURCE */
__END_DECLS

#endif	/* !_KERNEL && !_STANDALONE */
#endif /* !_COMPAT_SYS_TIME_H_ */
