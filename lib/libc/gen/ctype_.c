/*	$NetBSD: ctype_.c,v 1.20 2013/04/13 10:21:20 joerg Exp $	*/

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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
/*static char *sccsid = "from: @(#)ctype_.c	5.6 (Berkeley) 6/1/90";*/
#else
__RCSID("$NetBSD: ctype_.c,v 1.20 2013/04/13 10:21:20 joerg Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/ctype_bits.h>
#include <stdio.h>
#include "ctype_local.h"

#if EOF != -1
#error "EOF != -1"
#endif

#ifdef __BUILD_LEGACY
#define	_C	_COMPAT_C
#define	_S	_COMPAT_S
#define	_U	_COMPAT_U
#define	_X	_COMPAT_X
#define	_L	_COMPAT_L
#define	_P	_COMPAT_P
#define	_B	_COMPAT_B
#define	_N	_COMPAT_N

const unsigned char _C_compat_bsdctype[1 + _CTYPE_NUM_CHARS] = {
	0,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_S|_B,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_N,	_N,	_N,	_N,	_N,	_N,	_N,	_N,
	_N,	_N,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P,
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C
};

#undef _C
#undef _S
#undef _U
#undef _X
#undef _L
#undef _P
#undef _B
#undef _N

#ifdef __weak_alias
__weak_alias(_C_ctype_, _C_compat_bsdctype)
#endif

const unsigned char *_ctype_ = &_C_compat_bsdctype[0];
#endif /* __BUILD_LEGACY */

#define	_A	_CTYPE_A
#define	_BL	_CTYPE_BL
#define	_C	_CTYPE_C
#define	_D	_CTYPE_D
#define	_G	_CTYPE_G
#define	_L	_CTYPE_L
#define	_P	_CTYPE_P
#define	_R	_CTYPE_R
#define	_S	_CTYPE_S
#define	_U	_CTYPE_U
#define	_X	_CTYPE_X

const unsigned short _C_ctype_tab_[1 + _CTYPE_NUM_CHARS] = {
	0,
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	_C,		_BL|_C|_S,	_C|_S,		_C|_S,
	_C|_S,		_C|_S,		_C,		_C,
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	_BL|_R|_S,	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,
	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,
	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,
	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,
	_D|_G|_R|_X,	_D|_G|_R|_X,	_D|_G|_R|_X,	_D|_G|_R|_X,
	_D|_G|_R|_X,	_D|_G|_R|_X,	_D|_G|_R|_X,	_D|_G|_R|_X,
	_D|_G|_R|_X,	_D|_G|_R|_X,	_G|_R|_P,	_G|_R|_P,
	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,
	_G|_R|_P,	_A|_G|_R|_U|_X,	_A|_G|_R|_U|_X,	_A|_G|_R|_U|_X,
	_A|_G|_R|_U|_X,	_A|_G|_R|_U|_X,	_A|_G|_R|_U|_X,	_A|_G|_R|_U,
	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,
	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,
	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,
	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,
	_A|_G|_R|_U,	_A|_G|_R|_U,	_A|_G|_R|_U,	_G|_R|_P,
	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,
	_G|_R|_P,	_A|_G|_L|_R|_X,	_A|_G|_L|_R|_X,	_A|_G|_L|_R|_X,
	_A|_G|_L|_R|_X,	_A|_G|_L|_R|_X,	_A|_G|_L|_R|_X,	_A|_G|_L|_R,
	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,
	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,
	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,
	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,
	_A|_G|_L|_R,	_A|_G|_L|_R,	_A|_G|_L|_R,	_G|_R|_P,
	_G|_R|_P,	_G|_R|_P,	_G|_R|_P,	_C,
};

#undef _A
#undef _BL
#undef _C
#undef _D
#undef _G
#undef _L
#undef _P
#undef _R
#undef _S
#undef _U
#undef _X

const unsigned short *_ctype_tab_ = &_C_ctype_tab_[0];
