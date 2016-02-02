/*	$NetBSD: ansi.h,v 1.17 2014/02/24 16:57:57 christos Exp $	*/

/*
 * Copyright (c) 1990, 1993
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
 *	from: @(#)ansi.h	8.2 (Berkeley) 1/4/94
 */

#ifndef _ARM_ANSI_H_
#define _ARM_ANSI_H_

#include <sys/cdefs.h>

#include <machine/int_types.h>

/*
 * Types which are fundamental to the implementation and may appear in
 * more than one standard header are defined here.  Standard headers
 * then use:
 *	#ifdef	_BSD_SIZE_T_
 *	typedef	_BSD_SIZE_T_ size_t;
 *	#undef	_BSD_SIZE_T_
 *	#endif
 */
#define	_BSD_CLOCK_T_		unsigned int	/* clock() */
#ifdef __PTRDIFF_TYPE__
#define	_BSD_PTRDIFF_T_		__PTRDIFF_TYPE__ /* ptr1 - ptr2 */
#define	_BSD_SSIZE_T_		__PTRDIFF_TYPE__ /* byte count or error */
#else
#define	_BSD_PTRDIFF_T_		long int	/* ptr1 - ptr2 */
#define	_BSD_SSIZE_T_		long int	/* byte count or error */
#endif
#ifdef __SIZE_TYPE__
#define	_BSD_SIZE_T_		__SIZE_TYPE__	/* sizeof() */
#else
#define	_BSD_SIZE_T_		unsigned long int /* sizeof() */
#endif
#define	_BSD_TIME_T_		__int64_t	/* time() */
#define	_BSD_CLOCKID_T_		int		/* clockid_t */
#define	_BSD_TIMER_T_		int		/* timer_t */
#define	_BSD_SUSECONDS_T_	int		/* suseconds_t */
#define	_BSD_USECONDS_T_	unsigned int	/* useconds_t */
#ifdef __WCHAR_TYPE__
#define	_BSD_WCHAR_T_		__WCHAR_TYPE__	/* wchar_t */
#else
#define	_BSD_WCHAR_T_		int		/* wchar_t */
#endif
#ifdef __WINT_TYPE__
#define	_BSD_WINT_T_		__WINT_TYPE__	/* wint_t */
#else
#define	_BSD_WINT_T_		int		/* wint_t */
#endif

#endif	/* _ARM_ANSI_H_ */
