/*	$NetBSD: ps.h,v 1.26 2006/10/02 17:54:35 apb Exp $	*/

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
 *	@(#)ps.h	8.1 (Berkeley) 5/31/93
 */

#include <sys/queue.h>

#define	UNLIMITED	0	/* unlimited terminal width */

#define	PRINTMODE	0	/* print values */
#define	WIDTHMODE	1	/* determine width of column */

enum type {
	UNSPECIFIED,
	CHAR, UCHAR, SHORT, USHORT, INT, UINT, LONG, ULONG,
	KPTR, KPTR24, INT32, UINT32, SIGLIST, INT64, UINT64,
	TIMEVAL, CPUTIME, PCPU, VSIZE
};

/* Variables. */
typedef SIMPLEQ_HEAD(varlist, varent) VARLIST;

typedef struct varent {
	SIMPLEQ_ENTRY(varent) next;
	struct var *var;
} VARENT;

typedef struct var {
	const char *name;	/* name(s) of variable */
	const char *header;	/* header, possibly changed from default */
#define	COMM	0x01		/* needs exec arguments and environment (XXX) */
#define	ARGV0	0x02		/* only print argv[0] */
#define	LJUST	0x04		/* left adjust on output (trailing blanks) */
#define	INF127	0x08		/* 127 = infinity: if > 127, print 127. */
#define	LWP	0x10		/* dispatch to kinfo_lwp routine */
#define	UAREA	0x20		/* need to check p_uvalid */
#define	ALIAS	0x40		/* entry is alias for 'header' */
	u_int	flag;
				/* output routine */
	void	(*oproc)(void *, struct varent *, int);
	/*
	 * The following (optional) elements are hooks for passing information
	 * to the generic output routine: pvar (that which prints simple
	 * elements from struct kinfo_proc2).
	 */
	int	off;		/* offset in structure */
	enum	type type;	/* type of element */
	const char *fmt;	/* printf format */

	/* current longest element */
	int	width;		/* printing width */
	int64_t	longestp;	/* longest positive signed value */
	int64_t	longestn;	/* longest negative signed value */
	u_int64_t longestu;	/* longest unsigned value */
	double	longestpd;	/* longest positive double */
	double	longestnd;	/* longest negative double */
} VAR;

#define	OUTPUT(vent, ki, kl, mode) do {					\
	if ((vent)->var->flag & LWP)					\
		((vent)->var->oproc)((void *)(kl), (vent), (mode));	\
	else								\
		((vent)->var->oproc)((void *)(ki), (vent), (mode));	\
	} while (/*CONSTCOND*/ 0)

#include "extern.h"
