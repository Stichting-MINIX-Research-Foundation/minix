/*	$NetBSD: runetype_local.h,v 1.12 2010/06/20 02:23:15 tnozaki Exp $	*/

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
 *	@(#)rune.h	8.1 (Berkeley) 6/27/93
 *	@(#)runetype.h	8.1 (Berkeley) 6/2/93
 */

#ifndef	_RUNETYPE_LOCAL_H_
#define	_RUNETYPE_LOCAL_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdio.h>

#include "runetype_misc.h"

#define _RUNE_ISCACHED(c)	((c)>=0 && (c)<_CTYPE_CACHE_SIZE)


/*
 * expanded rune locale declaration.  local to the host.  host endian.
 */
typedef struct {
	__nbrune_t	re_min;		/* First rune of the range */
	__nbrune_t	re_max;		/* Last rune (inclusive) of the range */
	__nbrune_t	re_map;		/* What first maps to in maps */
	_RuneType	*re_rune_types;	/* Array of types in range */
} _RuneEntry;


typedef struct {
	uint32_t	rr_nranges;	/* Number of ranges stored */
	_RuneEntry	*rr_rune_ranges;
} _RuneRange;


/*
 * wctrans stuffs.
 */
typedef struct _WCTransEntry {
	const char	*te_name;
	__nbrune_t	*te_cached;
	_RuneRange	*te_extmap;
} _WCTransEntry;
#define _WCTRANS_INDEX_LOWER	0
#define _WCTRANS_INDEX_UPPER	1
#define _WCTRANS_NINDEXES	2

/*
 * wctype stuffs.
 */
typedef struct _WCTypeEntry {
	const char	*te_name;
	_RuneType	te_mask;
} _WCTypeEntry;
#define _WCTYPE_INDEX_ALNUM	0
#define _WCTYPE_INDEX_ALPHA	1
#define _WCTYPE_INDEX_BLANK	2
#define _WCTYPE_INDEX_CNTRL	3
#define _WCTYPE_INDEX_DIGIT	4
#define _WCTYPE_INDEX_GRAPH	5
#define _WCTYPE_INDEX_LOWER	6
#define _WCTYPE_INDEX_PRINT	7
#define _WCTYPE_INDEX_PUNCT	8
#define _WCTYPE_INDEX_SPACE	9
#define _WCTYPE_INDEX_UPPER	10
#define _WCTYPE_INDEX_XDIGIT	11
#define _WCTYPE_NINDEXES	12

/*
 * ctype stuffs
 */

typedef struct _RuneLocale {
	/*
	 * copied from _FileRuneLocale
	 */
	_RuneType	rl_runetype[_CTYPE_CACHE_SIZE];
	__nbrune_t	rl_maplower[_CTYPE_CACHE_SIZE];
	__nbrune_t	rl_mapupper[_CTYPE_CACHE_SIZE];
	_RuneRange	rl_runetype_ext;
	_RuneRange	rl_maplower_ext;
	_RuneRange	rl_mapupper_ext;

	void		*rl_variable;
	size_t		rl_variable_len;

	/*
	 * the following portion is generated on the fly
	 */
	const char			*rl_codeset;
	struct _citrus_ctype_rec	*rl_citrus_ctype;
	_WCTransEntry			rl_wctrans[_WCTRANS_NINDEXES];
	_WCTypeEntry			rl_wctype[_WCTYPE_NINDEXES];

	const unsigned char		*rl_ctype_tab;
	const short			*rl_tolower_tab;
	const short			*rl_toupper_tab;
} _RuneLocale;

/*
 * global variables
 */
extern const _RuneLocale _DefaultRuneLocale;
extern const _RuneLocale *_CurrentRuneLocale;

__BEGIN_DECLS
int _rune_load(const char * __restrict, size_t, _RuneLocale ** __restrict);
__END_DECLS

#endif	/* !_RUNETYPE_LOCAL_H_ */
