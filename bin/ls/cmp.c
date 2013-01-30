/*	$NetBSD: cmp.c,v 1.17 2003/08/07 09:05:14 agc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
#ifndef lint
#if 0
static char sccsid[] = "@(#)cmp.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: cmp.c,v 1.17 2003/08/07 09:05:14 agc Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <fts.h>
#include <string.h>

#include "ls.h"
#include "extern.h"

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) || \
    defined(_XOPEN_SOURCE) || defined(__NetBSD__)
#define ATIMENSEC_CMP(x, op, y) ((x)->st_atimensec op (y)->st_atimensec)
#define CTIMENSEC_CMP(x, op, y) ((x)->st_ctimensec op (y)->st_ctimensec)
#define MTIMENSEC_CMP(x, op, y) ((x)->st_mtimensec op (y)->st_mtimensec)
#else
#define ATIMENSEC_CMP(x, op, y) \
	((x)->st_atimespec.tv_nsec op (y)->st_atimespec.tv_nsec)
#define CTIMENSEC_CMP(x, op, y) \
	((x)->st_ctimespec.tv_nsec op (y)->st_ctimespec.tv_nsec)
#define MTIMENSEC_CMP(x, op, y) \
	((x)->st_mtimespec.tv_nsec op (y)->st_mtimespec.tv_nsec)
#endif

int
namecmp(const FTSENT *a, const FTSENT *b)
{

	return (strcmp(a->fts_name, b->fts_name));
}

int
revnamecmp(const FTSENT *a, const FTSENT *b)
{

	return (strcmp(b->fts_name, a->fts_name));
}

int
modcmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_mtime > a->fts_statp->st_mtime)
		return (1);
	else if (b->fts_statp->st_mtime < a->fts_statp->st_mtime)
		return (-1);
	else if (MTIMENSEC_CMP(b->fts_statp, >, a->fts_statp))
		return (1);
	else if (MTIMENSEC_CMP(b->fts_statp, <, a->fts_statp))
		return (-1);
	else
		return (namecmp(a, b));
}

int
revmodcmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_mtime > a->fts_statp->st_mtime)
		return (-1);
	else if (b->fts_statp->st_mtime < a->fts_statp->st_mtime)
		return (1);
	else if (MTIMENSEC_CMP(b->fts_statp, >, a->fts_statp))
		return (-1);
	else if (MTIMENSEC_CMP(b->fts_statp, <, a->fts_statp))
		return (1);
	else
		return (revnamecmp(a, b));
}

int
acccmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_atime > a->fts_statp->st_atime)
		return (1);
	else if (b->fts_statp->st_atime < a->fts_statp->st_atime)
		return (-1);
	else if (ATIMENSEC_CMP(b->fts_statp, >, a->fts_statp))
		return (1);
	else if (ATIMENSEC_CMP(b->fts_statp, <, a->fts_statp))
		return (-1);
	else
		return (namecmp(a, b));
}

int
revacccmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_atime > a->fts_statp->st_atime)
		return (-1);
	else if (b->fts_statp->st_atime < a->fts_statp->st_atime)
		return (1);
	else if (ATIMENSEC_CMP(b->fts_statp, >, a->fts_statp))
		return (-1);
	else if (ATIMENSEC_CMP(b->fts_statp, <, a->fts_statp))
		return (1);
	else
		return (revnamecmp(a, b));
}

int
statcmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_ctime > a->fts_statp->st_ctime)
		return (1);
	else if (b->fts_statp->st_ctime < a->fts_statp->st_ctime)
		return (-1);
	else if (CTIMENSEC_CMP(b->fts_statp, >, a->fts_statp))
		return (1);
	else if (CTIMENSEC_CMP(b->fts_statp, <, a->fts_statp))
		return (-1);
	else
		return (namecmp(a, b));
}

int
revstatcmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_ctime > a->fts_statp->st_ctime)
		return (-1);
	else if (b->fts_statp->st_ctime < a->fts_statp->st_ctime)
		return (1);
	else if (CTIMENSEC_CMP(b->fts_statp, >, a->fts_statp))
		return (-1);
	else if (CTIMENSEC_CMP(b->fts_statp, <, a->fts_statp))
		return (1);
	else
		return (revnamecmp(a, b));
}

int
sizecmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_size > a->fts_statp->st_size)
		return (1);
	if (b->fts_statp->st_size < a->fts_statp->st_size)
		return (-1);
	else
		return (namecmp(a, b));
}

int
revsizecmp(const FTSENT *a, const FTSENT *b)
{

	if (b->fts_statp->st_size > a->fts_statp->st_size)
		return (-1);
	if (b->fts_statp->st_size < a->fts_statp->st_size)
		return (1);
	else
		return (revnamecmp(a, b));
}
