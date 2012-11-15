/*	$NetBSD: limits.h,v 1.32 2012/03/28 17:04:41 christos Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
 *	@(#)limits.h	8.2 (Berkeley) 1/4/94
 */

#ifndef _LIMITS_H_
#define	_LIMITS_H_

#include <sys/featuretest.h>

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
#define	_POSIX_AIO_LISTIO_MAX	2
#define	_POSIX_AIO_MAX		1
#define	_POSIX_ARG_MAX		4096
#define	_POSIX_CHILD_MAX	25
#define	_POSIX_HOST_NAME_MAX	255
#define	_POSIX_LINK_MAX		8
#define	_POSIX_LOGIN_NAME_MAX	9
#define	_POSIX_MAX_CANON	255
#define	_POSIX_MAX_INPUT	255
#define	_POSIX_MQ_OPEN_MAX	8
#define	_POSIX_MQ_PRIO_MAX	32
#define	_POSIX_NAME_MAX		14
#define	_POSIX_NGROUPS_MAX	8
#define	_POSIX_OPEN_MAX		20
#define	_POSIX_PATH_MAX		256
#define	_POSIX_PIPE_BUF		512
#define	_POSIX_RE_DUP_MAX	255
#define	_POSIX_SSIZE_MAX	32767
#define	_POSIX_STREAM_MAX	8
#define	_POSIX_SYMLINK_MAX	255
#define	_POSIX_SYMLOOP_MAX	8

/*
 * We have not implemented these yet
 *
 * _POSIX_THREAD_ATTR_STACKADDR
 * _POSIX_THREAD_ATTR_STACKSIZE
 * _POSIX_THREAD_CPUTIME
 * _POSIX_THREAD_PRIORITY_SCHEDULING
 * _POSIX_THREAD_PRIO_INHERIT
 * _POSIX_THREAD_PRIO_PROTECT
 * _POSIX_THREAD_PROCESS_SHARED
 * _POSIX_THREAD_SAFE_FUNCTIONS
 * _POSIX_THREAD_SPORADIC_SERVER
 */

/*
 * The following 3 are not part of the standard
 * but left here for compatibility
 */
#define	_POSIX_THREAD_DESTRUCTOR_ITERATIONS	4
#define	_POSIX_THREAD_KEYS_MAX			256
#define	_POSIX_THREAD_THREADS_MAX		64

/*
 * These are the correct names, defined in terms of the above
 */
#define	PTHREAD_DESTRUCTOR_ITERATIONS 	_POSIX_THREAD_DESTRUCTOR_ITERATIONS
#define	PTHREAD_KEYS_MAX		_POSIX_THREAD_KEYS_MAX
/* Not yet: PTHREAD_STACK_MIN */
#define	PTHREAD_THREADS_MAX		_POSIX_THREAD_THREADS_MAX

#define	_POSIX_TIMER_MAX	32
#define	_POSIX_TTY_NAME_MAX	9
#define	_POSIX_TZNAME_MAX	6

#define	_POSIX2_BC_BASE_MAX	99
#define	_POSIX2_BC_DIM_MAX	2048
#define	_POSIX2_BC_SCALE_MAX	99
#define	_POSIX2_BC_STRING_MAX	1000
#define	_POSIX2_CHARCLASS_NAME_MAX	14
#define	_POSIX2_COLL_WEIGHTS_MAX	2
#define	_POSIX2_EXPR_NEST_MAX	32
#define	_POSIX2_LINE_MAX	2048
#define	_POSIX2_RE_DUP_MAX	255

/*
 * X/Open CAE Specifications,
 * adopted in IEEE Std 1003.1-2001 XSI.
 */
#if (_POSIX_C_SOURCE - 0) >= 200112L || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
#define	_XOPEN_IOV_MAX		16
#define	_XOPEN_NAME_MAX		256
#define	_XOPEN_PATH_MAX		1024

#define PASS_MAX		128		/* Legacy */

#define CHARCLASS_NAME_MAX	14
#define NL_ARGMAX		9
#define NL_LANGMAX		14
#define NL_MSGMAX		32767
#define NL_NMAX			1
#define NL_SETMAX		255
#define NL_TEXTMAX		2048

	/* IEEE Std 1003.1-2001 TSF */
#define	_GETGR_R_SIZE_MAX	1024
#define	_GETPW_R_SIZE_MAX	1024

/* Always ensure that this is consistent with <stdio.h> */
#ifndef TMP_MAX
#define TMP_MAX			308915776	/* Legacy */
#endif
#endif /* _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE || _NETBSD_SOURCE */

#endif /* _POSIX_C_SOURCE || _XOPEN_SOURCE || _NETBSD_SOURCE */

#define MB_LEN_MAX		32	/* Allow ISO/IEC 2022 */

/*
 * X/Open Extended API set 2 (a.k.a. C063)
 * This hides unimplemented functions from GNU configure until
 * we are done implementing them.
 */
#if !defined(_INCOMPLETE_XOPEN_C063)
#define __stub_linkat
#define __stub_renameat
#define __stub_mkfifoat
#define __stub_mknodat
#define __stub_mkdirat
#define __stub_faccessat
#define __stub_fchmodat
#define __stub_fchownat
#define __stub_fexecve
#define __stub_fstatat
#define __stub_utimensat
#define __stub_openat
#define __stub_readlinkat
#define __stub_symlinkat
#define __stub_unlinkat
#endif

#include <machine/limits.h>

#ifdef __CHAR_UNSIGNED__
# define CHAR_MIN     0
# define CHAR_MAX     UCHAR_MAX
#else
# define CHAR_MIN     SCHAR_MIN
# define CHAR_MAX     SCHAR_MAX
#endif

#ifdef __minix
#define SYMLOOP_MAX		16
#define SYMLINK_MAX		1024
#endif /* __minix */

#include <sys/syslimits.h>

#endif /* !_LIMITS_H_ */
