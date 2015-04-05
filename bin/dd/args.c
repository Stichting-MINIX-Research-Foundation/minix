/*	$NetBSD: args.c,v 1.38 2013/07/17 12:55:48 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
static char sccsid[] = "@(#)args.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: args.c,v 1.38 2013/07/17 12:55:48 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dd.h"
#include "extern.h"

static int	c_arg(const void *, const void *);

#ifdef NO_MSGFMT
static void	f_msgfmt(char *) __dead;
#else
static void	f_msgfmt(char *);
#endif /* NO_MSGFMT */

#ifdef NO_CONV
static void	f_conv(char *) __dead;
#else
static void	f_conv(char *);
static int	c_conv(const void *, const void *);
#endif /* NO_CONV */

static void	f_bs(char *);
static void	f_cbs(char *);
static void	f_count(char *);
static void	f_files(char *);
static void	f_ibs(char *);
static void	f_if(char *);
static void	f_obs(char *);
static void	f_of(char *);
static void	f_seek(char *);
static void	f_skip(char *);
static void	f_progress(char *);

static const struct arg {
	const char *name;
	void (*f)(char *);
	u_int set, noset;
} args[] = {
     /* the array needs to be sorted by the first column so
	bsearch() can be used to find commands quickly */
	{ "bs",		f_bs,		C_BS,	 C_BS|C_IBS|C_OBS|C_OSYNC },
	{ "cbs",	f_cbs,		C_CBS,	 C_CBS },
	{ "conv",	f_conv,		0,	 0 },
	{ "count",	f_count,	C_COUNT, C_COUNT },
	{ "files",	f_files,	C_FILES, C_FILES },
	{ "ibs",	f_ibs,		C_IBS,	 C_BS|C_IBS },
	{ "if",		f_if,		C_IF,	 C_IF },
	{ "iseek",	f_skip,		C_SKIP,	 C_SKIP },
	{ "msgfmt",	f_msgfmt,	0,	 0 },
	{ "obs",	f_obs,		C_OBS,	 C_BS|C_OBS },
	{ "of",		f_of,		C_OF,	 C_OF },
	{ "oseek",	f_seek,		C_SEEK,	 C_SEEK },
	{ "progress",	f_progress,	0,	 0 },
	{ "seek",	f_seek,		C_SEEK,	 C_SEEK },
	{ "skip",	f_skip,		C_SKIP,	 C_SKIP },
};

/*
 * args -- parse JCL syntax of dd.
 */
void
jcl(char **argv)
{
	struct arg *ap, tmp;
	char *oper, *arg;

	in.dbsz = out.dbsz = 512;

	while ((oper = *++argv) != NULL) {
		if ((oper = strdup(oper)) == NULL) {
			errx(EXIT_FAILURE,
			    "unable to allocate space for the argument %s",
			    *argv);
			/* NOTREACHED */
		}
		if ((arg = strchr(oper, '=')) == NULL) {
			errx(EXIT_FAILURE, "unknown operand %s", oper);
			/* NOTREACHED */
		}
		*arg++ = '\0';
		if (!*arg) {
			errx(EXIT_FAILURE, "no value specified for %s", oper);
			/* NOTREACHED */
		}
		tmp.name = oper;
		if (!(ap = bsearch(&tmp, args,
		    __arraycount(args), sizeof(*args), c_arg))) {
			errx(EXIT_FAILURE, "unknown operand %s", tmp.name);
			/* NOTREACHED */
		}
		if (ddflags & ap->noset) {
			errx(EXIT_FAILURE,
			    "%s: illegal argument combination or already set",
			    tmp.name);
			/* NOTREACHED */
		}
		ddflags |= ap->set;
		ap->f(arg);
	}

	/* Final sanity checks. */

	if (ddflags & C_BS) {
		/*
		 * Bs is turned off by any conversion -- we assume the user
		 * just wanted to set both the input and output block sizes
		 * and didn't want the bs semantics, so we don't warn.
		 */
		if (ddflags & (C_BLOCK | C_LCASE | C_SWAB | C_UCASE |
		    C_UNBLOCK | C_OSYNC | C_ASCII | C_EBCDIC | C_SPARSE)) {
			ddflags &= ~C_BS;
			ddflags |= C_IBS|C_OBS;
		}

		/* Bs supersedes ibs and obs. */
		if (ddflags & C_BS && ddflags & (C_IBS|C_OBS))
			warnx("bs supersedes ibs and obs");
	}

	/*
	 * Ascii/ebcdic and cbs implies block/unblock.
	 * Block/unblock requires cbs and vice-versa.
	 */
	if (ddflags & (C_BLOCK|C_UNBLOCK)) {
		if (!(ddflags & C_CBS)) {
			errx(EXIT_FAILURE, "record operations require cbs");
			/* NOTREACHED */
		}
		cfunc = ddflags & C_BLOCK ? block : unblock;
	} else if (ddflags & C_CBS) {
		if (ddflags & (C_ASCII|C_EBCDIC)) {
			if (ddflags & C_ASCII) {
				ddflags |= C_UNBLOCK;
				cfunc = unblock;
			} else {
				ddflags |= C_BLOCK;
				cfunc = block;
			}
		} else {
			errx(EXIT_FAILURE,
			    "cbs meaningless if not doing record operations");
			/* NOTREACHED */
		}
	} else
		cfunc = def;

	/* Read, write and seek calls take off_t as arguments.
	 *
	 * The following check is not done because an off_t is a quad
	 *  for current NetBSD implementations.
	 *
	 * if (in.offset > INT_MAX/in.dbsz || out.offset > INT_MAX/out.dbsz)
	 *	errx(1, "seek offsets cannot be larger than %d", INT_MAX);
	 */
}

static int
c_arg(const void *a, const void *b)
{

	return (strcmp(((const struct arg *)a)->name,
	    ((const struct arg *)b)->name));
}

static void
f_bs(char *arg)
{

	in.dbsz = out.dbsz = strsuftoll("block size", arg, 1, UINT_MAX);
}

static void
f_cbs(char *arg)
{

	cbsz = strsuftoll("conversion record size", arg, 1, UINT_MAX);
}

static void
f_count(char *arg)
{

	cpy_cnt = strsuftoll("block count", arg, 0, LLONG_MAX);
	if (!cpy_cnt)
		terminate(0);
}

static void
f_files(char *arg)
{

	files_cnt = (u_int)strsuftoll("file count", arg, 0, UINT_MAX);
	if (!files_cnt)
		terminate(0);
}

static void
f_ibs(char *arg)
{

	if (!(ddflags & C_BS))
		in.dbsz = strsuftoll("input block size", arg, 1, UINT_MAX);
}

static void
f_if(char *arg)
{

	in.name = arg;
}

#ifdef NO_MSGFMT
/* Build a small version (i.e. for a ramdisk root) */
static void
f_msgfmt(char *arg)
{

	errx(EXIT_FAILURE, "msgfmt option disabled");
	/* NOTREACHED */
}
#else	/* NO_MSGFMT */
static void
f_msgfmt(char *arg)
{

	/*
	 * If the format string is not valid, dd_write_msg() will print
	 * an error and exit.
	 */
	dd_write_msg(arg, 0);

	msgfmt = arg;
}
#endif	/* NO_MSGFMT */

static void
f_obs(char *arg)
{

	if (!(ddflags & C_BS))
		out.dbsz = strsuftoll("output block size", arg, 1, UINT_MAX);
}

static void
f_of(char *arg)
{

	out.name = arg;
}

static void
f_seek(char *arg)
{

	out.offset = strsuftoll("seek blocks", arg, 0, LLONG_MAX);
}

static void
f_skip(char *arg)
{

	in.offset = strsuftoll("skip blocks", arg, 0, LLONG_MAX);
}

static void
f_progress(char *arg)
{

	progress = strsuftoll("progress blocks", arg, 0, LLONG_MAX);
}

#ifdef	NO_CONV
/* Build a small version (i.e. for a ramdisk root) */
static void
f_conv(char *arg)
{

	errx(EXIT_FAILURE, "conv option disabled");
	/* NOTREACHED */
}
#else	/* NO_CONV */

static const struct conv {
	const char *name;
	u_int set, noset;
	const u_char *ctab;
} clist[] = {
	{ "ascii",	C_ASCII,	C_EBCDIC,	e2a_POSIX },
	{ "block",	C_BLOCK,	C_UNBLOCK,	NULL },
	{ "ebcdic",	C_EBCDIC,	C_ASCII,	a2e_POSIX },
	{ "ibm",	C_EBCDIC,	C_ASCII,	a2ibm_POSIX },
	{ "lcase",	C_LCASE,	C_UCASE,	NULL },
	{ "noerror",	C_NOERROR,	0,		NULL },
	{ "notrunc",	C_NOTRUNC,	0,		NULL },
	{ "oldascii",	C_ASCII,	C_EBCDIC,	e2a_32V },
	{ "oldebcdic",	C_EBCDIC,	C_ASCII,	a2e_32V },
	{ "oldibm",	C_EBCDIC,	C_ASCII,	a2ibm_32V },
	{ "osync",	C_OSYNC,	C_BS,		NULL },
	{ "sparse",	C_SPARSE,	0,		NULL },
	{ "swab",	C_SWAB,		0,		NULL },
	{ "sync",	C_SYNC,		0,		NULL },
	{ "ucase",	C_UCASE,	C_LCASE,	NULL },
	{ "unblock",	C_UNBLOCK,	C_BLOCK,	NULL },
	/* If you add items to this table, be sure to add the
	 * conversions to the C_BS check in the jcl routine above.
	 */
};

static void
f_conv(char *arg)
{
	struct conv *cp, tmp;

	while (arg != NULL) {
		tmp.name = strsep(&arg, ",");
		if (!(cp = bsearch(&tmp, clist,
		    __arraycount(clist), sizeof(*clist), c_conv))) {
			errx(EXIT_FAILURE, "unknown conversion %s", tmp.name);
			/* NOTREACHED */
		}
		if (ddflags & cp->noset) {
			errx(EXIT_FAILURE,
			    "%s: illegal conversion combination", tmp.name);
			/* NOTREACHED */
		}
		ddflags |= cp->set;
		if (cp->ctab)
			ctab = cp->ctab;
	}
}

static int
c_conv(const void *a, const void *b)
{

	return (strcmp(((const struct conv *)a)->name,
	    ((const struct conv *)b)->name));
}

#endif	/* NO_CONV */
