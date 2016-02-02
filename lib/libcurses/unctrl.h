/*	$NetBSD: unctrl.h,v 1.5 2015/05/28 06:28:37 wiz Exp $	*/

/*
 * Copyright (c) 1982, 1993
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
 *	@(#)unctrl.h	8.1 (Berkeley) 5/31/93
 */

#ifndef _UNCTRL_H_
#define _UNCTRL_H_

#include <sys/cdefs.h>
#ifdef HAVE_WCHAR
#include <wchar.h>
#include <curses.h>
#endif /* HAVE_WCHAR */

__BEGIN_DECLS
extern const char * const  __unctrl[];		/* Control strings. */
extern const unsigned char __unctrllen[];	/* Control strings length. */
#ifdef HAVE_WCHAR
extern const wchar_t * const  __wunctrl[];	/* Wide char control strings. */
#endif /* HAVE_WCHAR */
__END_DECLS

/* 8-bit ASCII characters. */
#define	unctrl(c)		__unctrl[((unsigned char)c) & 0xff]
#define	unctrllen(c)	__unctrllen[((unsigned char)c) & 0xff]

#ifdef HAVE_WCHAR
#define	wunctrl(wc)		__wunctrl[( int )((wc)->vals[ 0 ]) & 0xff]
#endif /* HAVE_WCHAR */
#endif /* _UNCTRL_H_ */
