/* $NetBSD: ctype_inline.h,v 1.2 2010/12/14 02:28:57 joerg Exp $ */

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)ctype.h	5.3 (Berkeley) 4/3/91
 *	NetBSD: ctype.h,v 1.30 2010/05/22 06:38:15 tnozaki Exp
 */

#ifndef _CTYPE_INLINE_H_
#define _CTYPE_INLINE_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>

#include <sys/ctype_bits.h>
/* LSC: cast to unsigned char in order to prevent char as indice errors. */
#define	isdigit(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & _CTYPE_N))
#define	islower(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & _CTYPE_L))
#define	isspace(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & _CTYPE_S))
#define	ispunct(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & _CTYPE_P))
#define	isupper(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & _CTYPE_U))
#define	isalpha(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & (_CTYPE_U|_CTYPE_L)))
#define	isxdigit(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & (_CTYPE_N|_CTYPE_X)))
#define	isalnum(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & (_CTYPE_U|_CTYPE_L|_CTYPE_N)))
#define	isprint(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & (_CTYPE_P|_CTYPE_U|_CTYPE_L|_CTYPE_N|_CTYPE_B)))
#define	isgraph(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & (_CTYPE_P|_CTYPE_U|_CTYPE_L|_CTYPE_N)))
#define	iscntrl(c)	((int)((_ctype_ + 1)[(unsigned char)(c)] & _CTYPE_C))
#define	tolower(c)	((int)((_tolower_tab_ + 1)[(unsigned char)(c)]))
#define	toupper(c)	((int)((_toupper_tab_ + 1)[(unsigned char)(c)]))

#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
#define	isascii(c)	((unsigned)(c) <= 0177)
#define	toascii(c)	((c) & 0177)
#define _tolower(c)	((c) - 'A' + 'a')
#define _toupper(c)	((c) - 'a' + 'A')
#endif

#if defined(_ISO_C99_SOURCE) || (_POSIX_C_SOURCE - 0) > 200112L || \
    (_XOPEN_SOURCE - 0) > 600 || defined(_NETBSD_SOURCE)

/*
 * isblank() is implemented as C function, due to insufficient bitwidth in
 * _ctype_.  Note that _B does not mean isblank - it means isprint && !isgraph.
 */
#if 0
#define isblank(c)	((int)((_ctype_ + 1)[(c)] & _B))
#endif

#endif

#endif /* !_CTYPE_INLINE_H_ */
