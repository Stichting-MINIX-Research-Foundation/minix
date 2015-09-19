/*	$NetBSD: option.c,v 1.26 2007/02/06 15:33:22 perry Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
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
static char sccsid[] = "from: @(#)option.c	8.2 (Berkeley) 4/16/94";
#else
__RCSID("$NetBSD: option.c,v 1.26 2007/02/06 15:33:22 perry Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "find.h"

int typecompare(const void *, const void *);
static OPTION *option(char *);

/* NB: the following table must be sorted lexically. */
static OPTION const options[] = {
	{ "!",		N_NOT,		c_not,		0 },
	{ "(",		N_OPENPAREN,	c_openparen,	0 },
	{ ")",		N_CLOSEPAREN,	c_closeparen,	0 },
	{ "-a",		N_AND,		c_null,		0 },
	{ "-amin",	N_AMIN,		c_amin,		1 },
	{ "-and",	N_AND,		c_null,		0 },
	{ "-anewer",	N_ANEWER,	c_anewer,	1 },
	{ "-atime",	N_ATIME,	c_atime,	1 },
	{ "-cmin",	N_CMIN,		c_cmin,		1 },
	{ "-cnewer",	N_CNEWER,	c_cnewer,	1 },
	{ "-ctime",	N_CTIME,	c_ctime,	1 },
	{ "-delete",	N_DELETE,	c_delete,	0 },
	{ "-depth",	N_DEPTH,	c_depth,	0 },
	{ "-empty",	N_EMPTY,	c_empty,	0 },
	{ "-exec",	N_EXEC,		c_exec,		1 },
	{ "-execdir",	N_EXECDIR,	c_execdir,	1 },
	{ "-exit",	N_EXIT,		c_exit,		0 },
	{ "-false",	N_FALSE,	c_false,	0 },
	{ "-flags",	N_FLAGS,	c_flags,	1 },
	{ "-follow",	N_FOLLOW,	c_follow,	0 },
	{ "-fprint",	N_FPRINT,	c_fprint,	1 },
	{ "-fstype",	N_FSTYPE,	c_fstype,	1 },
	{ "-group",	N_GROUP,	c_group,	1 },
	{ "-iname",	N_INAME,	c_iname,	1 },
	{ "-inum",	N_INUM,		c_inum,		1 },
	{ "-iregex",	N_IREGEX,	c_iregex,	1 },
	{ "-links",	N_LINKS,	c_links,	1 },
	{ "-ls",	N_LS,		c_ls,		0 },
	{ "-maxdepth",	N_MAXDEPTH,	c_maxdepth,	1 },
	{ "-mindepth",	N_MINDEPTH,	c_mindepth,	1 },
	{ "-mmin",	N_MMIN,		c_mmin,		1 },
	{ "-mtime",	N_MTIME,	c_mtime,	1 },
	{ "-name",	N_NAME,		c_name,		1 },
	{ "-newer",	N_NEWER,	c_newer,	1 },
	{ "-nogroup",	N_NOGROUP,	c_nogroup,	0 },
	{ "-nouser",	N_NOUSER,	c_nouser,	0 },
	{ "-o",		N_OR,		c_or,		0 },
	{ "-ok",	N_OK,		c_exec,		1 },
	{ "-or",	N_OR,		c_or,		0 },
	{ "-path", 	N_PATH,		c_path,		1 },
	{ "-perm",	N_PERM,		c_perm,		1 },
	{ "-print",	N_PRINT,	c_print,	0 },
	{ "-print0",	N_PRINT0,	c_print0,	0 },
	{ "-printx",	N_PRINTX,	c_printx,	0 },
	{ "-prune",	N_PRUNE,	c_prune,	0 },
	{ "-regex",	N_REGEX,	c_regex,	1 },
	{ "-rm",	N_DELETE,	c_delete,	0 },
	{ "-size",	N_SIZE,		c_size,		1 },
	{ "-type",	N_TYPE,		c_type,		1 },
	{ "-user",	N_USER,		c_user,		1 },
	{ "-xdev",	N_XDEV,		c_xdev,		0 }
};

/*
 * find_create --
 *	create a node corresponding to a command line argument.
 *
 * TODO:
 *	add create/process function pointers to node, so we can skip
 *	this switch stuff.
 */
PLAN *
find_create(char ***argvp)
{
	OPTION *p;
	PLAN *new;
	char **argv;

	argv = *argvp;

	if ((p = option(*argv)) == NULL)
		errx(1, "%s: unknown option", *argv);
	++argv;
	if (p->arg && !*argv)
		errx(1, "%s: requires additional arguments", *--argv);

	new = (p->create)(&argv, p->token == N_OK);

	*argvp = argv;
	return (new);
}

static OPTION *
option(char *name)
{
	OPTION tmp;

	tmp.name = name;
	return ((OPTION *)bsearch(&tmp, options,
	    sizeof(options)/sizeof(OPTION), sizeof(OPTION), typecompare));
}

int
typecompare(const void *a, const void *b)
{

	return (strcmp(((const OPTION *)a)->name, ((const OPTION *)b)->name));
}
