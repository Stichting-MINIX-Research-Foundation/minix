/*	$NetBSD: sigtypes.h,v 1.2 2005/12/11 12:20:29 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)signal.h	8.4 (Berkeley) 5/4/95
 */

#ifndef	_COMPAT_SYS_SIGTYPES_H_
#define	_COMPAT_SYS_SIGTYPES_H_

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd32.h"
#endif

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)

typedef unsigned int sigset13_t;

/*
 * Macro for manipulating signal masks.
 */
#define __sigmask13(n)		(1 << ((n) - 1))
#define	__sigaddset13(s, n)	(*(s) |= __sigmask13(n))
#define	__sigdelset13(s, n)	(*(s) &= ~__sigmask13(n))
#define	__sigismember13(s, n)	(*(s) & __sigmask13(n))
#define	__sigemptyset13(s)	(*(s) = 0)
#define	__sigfillset13(s)	(*(s) = ~(sigset13_t)0)

/* Not strictly a defined type, but is logically associated with stack_t. */
struct sigaltstack13 {
	char	*ss_sp;			/* signal stack base */
	int	ss_size;		/* signal stack length */
	int	ss_flags;		/* SS_DISABLE and/or SS_ONSTACK */
};

#endif	/* _POSIX_C_SOURCE || _XOPEN_SOURCE || ... */

#if defined(COMPAT_NETBSD32) && defined(_KERNEL)

struct __sigaltstack32 {
	uint32_t	ss_sp;
	uint32_t	ss_size;
	int32_t		ss_flags;
};

typedef struct __sigaltstack32 stack32_t;

#endif /* COMPAT_NETBSD32 && _KERNEL */


#endif	/* !_COMPAT_SYS_SIGTYPES_H_ */
