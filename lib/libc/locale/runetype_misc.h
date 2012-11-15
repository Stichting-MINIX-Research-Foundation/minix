/* $NetBSD: runetype_misc.h,v 1.3 2012/01/18 14:22:27 joerg Exp $ */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
 *	@(#)runetype.h	8.1 (Berkeley) 6/2/93
 */

#ifndef	_RUNETYPE_MISC_H_
#define	_RUNETYPE_MISC_H_

#include <sys/ctype_bits.h>
#include "runetype_file.h"

static __inline int
_runetype_to_ctype(_RuneType bits)
{
	int ret;

	if (bits == (_RuneType)0)
		return 0;
	ret = 0;
	if (bits & _RUNETYPE_U)
		ret |= _CTYPE_U;
	if (bits & _RUNETYPE_L)
		ret |= _CTYPE_L;
	if (bits & _RUNETYPE_D)
		ret |= _CTYPE_N;
	if (bits & _RUNETYPE_S)
		ret |= _CTYPE_S;
	if (bits & _RUNETYPE_P)
		ret |= _CTYPE_P;
	if (bits & _RUNETYPE_C)
		ret |= _CTYPE_C;
	if (bits & _RUNETYPE_X)
		ret |= _CTYPE_X;
	/*
	 * TWEAK!  _B has been used incorrectly (or with older
	 * declaration) in ctype.h isprint() macro.
	 * _B does not mean isblank, it means "isprint && !isgraph".
	 * the following is okay since isblank() was hardcoded in
	 * function (i.e. isblank() is inherently locale unfriendly).
	 */
#if 1
	if ((bits & (_RUNETYPE_R | _RUNETYPE_G)) == _RUNETYPE_R)
		ret |= _CTYPE_B;
#else
	if (bits & _RUNETYPE_B)
		ret |= _CTYPE_B;
#endif
	return ret;
}

#endif	/* !_RUNETYPE_MISC_H_ */
