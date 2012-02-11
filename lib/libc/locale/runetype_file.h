/* $NetBSD: runetype_file.h,v 1.3 2010/06/20 02:23:15 tnozaki Exp $ */

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

#ifndef	_RUNETYPE_FILE_H_
#define	_RUNETYPE_FILE_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include "ctype_local.h"

/* for cross host tools on older systems */
#ifndef UINT32_C
/* assumes sizeof(unsigned int)>=4 */
#define UINT32_C(c) ((uint32_t)(c##U))
#endif

typedef uint32_t	__nbrune_t;
typedef uint64_t	__runepad_t;

#define _DEFAULT_INVALID_RUNE ((__nbrune_t)-3)

/*
 * The lower 8 bits of runetype[] contain the digit value of the rune.
 */
typedef uint32_t _RuneType;
#define	_RUNETYPE_A	UINT32_C(0x00000100)	/* Alpha */
#define	_RUNETYPE_C	UINT32_C(0x00000200)	/* Control */
#define	_RUNETYPE_D	UINT32_C(0x00000400)	/* Digit */
#define	_RUNETYPE_G	UINT32_C(0x00000800)	/* Graph */
#define	_RUNETYPE_L	UINT32_C(0x00001000)	/* Lower */
#define	_RUNETYPE_P	UINT32_C(0x00002000)	/* Punct */
#define	_RUNETYPE_S	UINT32_C(0x00004000)	/* Space */
#define	_RUNETYPE_U	UINT32_C(0x00008000)	/* Upper */
#define	_RUNETYPE_X	UINT32_C(0x00010000)	/* X digit */
#define	_RUNETYPE_B	UINT32_C(0x00020000)	/* Blank */
#define	_RUNETYPE_R	UINT32_C(0x00040000)	/* Print */
#define	_RUNETYPE_I	UINT32_C(0x00080000)	/* Ideogram */
#define	_RUNETYPE_T	UINT32_C(0x00100000)	/* Special */
#define	_RUNETYPE_Q	UINT32_C(0x00200000)	/* Phonogram */
#define	_RUNETYPE_SWM	UINT32_C(0xc0000000)/* Mask to get screen width data */
#define	_RUNETYPE_SWS	30		/* Bits to shift to get width */
#define	_RUNETYPE_SW0	UINT32_C(0x20000000)	/* 0 width character */
#define	_RUNETYPE_SW1	UINT32_C(0x40000000)	/* 1 width character */
#define	_RUNETYPE_SW2	UINT32_C(0x80000000)	/* 2 width character */
#define	_RUNETYPE_SW3	UINT32_C(0xc0000000)	/* 3 width character */

/*
 * rune file format.  network endian.
 */
typedef struct {
	int32_t		fre_min;	/* First rune of the range */
	int32_t		fre_max;	/* Last rune (inclusive) of the range */
	int32_t		fre_map;	/* What first maps to in maps */
	uint32_t	fre_pad1;	/* backward compatibility */
	__runepad_t	fre_pad2;	/* backward compatibility */
} __packed _FileRuneEntry;


typedef struct {
	uint32_t	frr_nranges;	/* Number of ranges stored */
	uint32_t	frr_pad1;	/* backward compatibility */
	__runepad_t	frr_pad2;	/* backward compatibility */
} __packed _FileRuneRange;


typedef struct {
	char		frl_magic[8];	/* Magic saying what version we are */
	char		frl_encoding[32];/* ASCII name of this encoding */

	__runepad_t	frl_pad1;	/* backward compatibility */
	__runepad_t	frl_pad2;	/* backward compatibility */
	int32_t		frl_invalid_rune;
	uint32_t	frl_pad3;	/* backward compatibility */

	_RuneType	frl_runetype[_CTYPE_CACHE_SIZE];
	int32_t		frl_maplower[_CTYPE_CACHE_SIZE];
	int32_t		frl_mapupper[_CTYPE_CACHE_SIZE];

	/*
	 * The following are to deal with Runes larger than _CTYPE_CACHE_SIZE - 1.
	 * Their data is actually contiguous with this structure so as to make
	 * it easier to read/write from/to disk.
	 */
	_FileRuneRange	frl_runetype_ext;
	_FileRuneRange	frl_maplower_ext;
	_FileRuneRange	frl_mapupper_ext;

	__runepad_t	frl_pad4;	/* backward compatibility */
	int32_t		frl_variable_len;/* how long that data is */
	uint32_t	frl_pad5;	/* backward compatibility */

	/* variable size data follows */
} __packed _FileRuneLocale;


/* magic number for LC_CTYPE (rune)locale declaration */
#define	_RUNECT10_MAGIC	"RuneCT10"	/* Indicates version 0 of RuneLocale */

/* codeset tag */
#define _RUNE_CODESET "CODESET="

#endif	/* !_RUNETYPE_FILE_H_ */
