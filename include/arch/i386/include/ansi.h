/*	$NetBSD: ansi.h,v 1.24 2010/03/27 22:14:09 tnozaki Exp $	*/

/*-
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
 *	@(#)ansi.h	8.2 (Berkeley) 1/4/94
 */

/* These types are Minix specific. */

#ifndef	_I386_ANSI_H_
#define	_I386_ANSI_H_

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

#define _BSD_CLOCK_T_		long

#if _EM_PSIZE == _EM_WSIZE
#define _BSD_PTRDIFF_T_ 	int
#else /* _EM_PSIZE == _EM_LSIZE */
#define _BSD_PTRDIFF_T_ 	long
#endif

#define _BSD_SIZE_T_		unsigned int
#define	_BSD_SSIZE_T_		int
#define	_BSD_TIME_T_		long		/* time() */
#if __GNUC_PREREQ__(2, 96)
#define	_BSD_VA_LIST_		__builtin_va_list /* GCC built-in type */
#else
#define	_BSD_VA_LIST_		char *		/* va_list */
#endif
#define	_BSD_CLOCKID_T_		int		/* clockid_t */
#define	_BSD_TIMER_T_		int		/* timer_t */
#define	_BSD_SUSECONDS_T_	long		/* suseconds_t */
#define	_BSD_USECONDS_T_	long		/* useconds_t */
#define	_BSD_WCHAR_T_		int		/* wchar_t */
#define	_BSD_WINT_T_		int		/* wint_t */

#endif	/* _I386_ANSI_H_ */
