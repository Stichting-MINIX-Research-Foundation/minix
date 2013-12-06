/* $NetBSD: ctype_local.h,v 1.5 2013/04/13 10:21:20 joerg Exp $ */

/*-
 * Copyright (c) 2010 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _CTYPE_LOCAL_H_
#define _CTYPE_LOCAL_H_

#include <limits.h>

#define _CTYPE_NUM_CHARS	(1 << CHAR_BIT)
#define _CTYPE_CACHE_SIZE	(1 << 8)

#define _COMPAT_U	0x01
#define _COMPAT_L	0x02
#define _COMPAT_N	0x04
#define _COMPAT_S	0x08
#define _COMPAT_P	0x10
#define _COMPAT_C	0x20
#define _COMPAT_X	0x40
#define _COMPAT_B	0x80

extern const unsigned short _C_ctype_tab_[];
extern const short _C_toupper_tab_[];
extern const short _C_tolower_tab_[];

#ifdef __BUILD_LEGACY
extern const unsigned char	*_ctype_;
extern const unsigned char	_C_compat_bsdctype[];
#endif

#endif /*_CTYPE_LOCAL_H_*/
