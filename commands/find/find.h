/*	$NetBSD: find.h,v 1.24 2007/02/06 13:25:01 elad Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
 *
 *	from: @(#)find.h	8.1 (Berkeley) 6/6/93
 */

#include <regex.h>

/* node type */
enum ntype {
	N_AND = 1, 				/* must start > 0 */
	N_AMIN, N_ANEWER, N_ATIME, N_CLOSEPAREN, N_CMIN, N_CNEWER, N_CTIME,
	N_DEPTH, N_EMPTY, N_EXEC, N_EXECDIR, N_EXIT, N_EXPR, N_FALSE,
	N_FOLLOW, N_FPRINT,
#ifndef _MINIX
	N_FSTYPE,
	N_FLAGS,
#endif
	N_GROUP,
	N_INAME, N_INUM, N_IREGEX, N_LINKS, N_LS, N_MINDEPTH, N_MAXDEPTH,
	N_MMIN, N_MTIME, N_NAME, N_NEWER, N_NOGROUP, N_NOT, N_NOUSER, N_OK,
	N_OPENPAREN, N_OR, N_PATH, N_PERM, N_PRINT, N_PRINT0, N_PRINTX,
	N_PRUNE, N_REGEX, N_SIZE, N_TYPE, N_USER, N_XDEV, N_DELETE
};

#ifndef __NBSD_LIBC
typedef unsigned int u_int32_t;
#endif

/* node definition */
typedef struct _plandata {
	struct _plandata *next;			/* next node */
	int (*eval)(struct _plandata *, FTSENT *);
						/* node evaluation function */
#define	F_EQUAL		1			/* [acm]time inum links size */
#define	F_LESSTHAN	2
#define	F_GREATER	3
#define	F_NEEDOK	1			/* exec ok */
#define	F_PLUSSET	2			/* -exec ... {} + */
#define	F_MTFLAG	1			/* fstype */
#define	F_MTTYPE	2
#define	F_ATLEAST	1			/* perm */
#define	F_ANY		2			/* perm */
	int flags;				/* private flags */
	enum ntype type;			/* plan node type */
	union {
		u_int32_t _f_data;		/* flags */
		gid_t _g_data;			/* gid */
		ino_t _i_data;			/* inode */
		mode_t _m_data;			/* mode mask */
		nlink_t _l_data;		/* link count */
		off_t _o_data;			/* file size */
		time_t _t_data;			/* time value */
		uid_t _u_data;			/* uid */
		short _mt_data;			/* mount flags */
		struct _plandata *_p_data[2];	/* PLAN trees */
		struct _ex {
			char **_e_argv;		/* argv array */
			char **_e_orig;		/* original strings */
			int *_e_len;		/* allocated length */
			char **_ep_bxp;		/* ptr to 1st addt'l arg */
			char *_ep_p;		/* current buffer pointer */
			char *_ep_bbp;		/* begin buffer pointer */
			char *_ep_ebp;		/* end buffer pointer */
			int _ep_maxargs;	/* max #args */
			int _ep_narg;		/* # addt'l args */
			int _ep_rval;		/* return value */
		} ex;
		char *_a_data[2];		/* array of char pointers */
		char *_c_data;			/* char pointer */
		int _exit_val;			/* exit value */
		int _max_data;			/* tree depth */
		int _min_data;			/* tree depth */
		regex_t _regexp_data;		/* compiled regexp */
		FILE *_fprint_file;		/* file stream for -fprint */
	} p_un;
} PLAN;
#define	a_data		p_un._a_data
#define	c_data		p_un._c_data
#define	i_data		p_un._i_data
#define	f_data		p_un._f_data
#define	g_data		p_un._g_data
#define	l_data		p_un._l_data
#define	m_data		p_un._m_data
#define	mt_data		p_un._mt_data
#define	o_data		p_un._o_data
#define	p_data		p_un._p_data
#define	t_data		p_un._t_data
#define	u_data		p_un._u_data
#define	e_argv		p_un.ex._e_argv
#define	e_orig		p_un.ex._e_orig
#define	e_len		p_un.ex._e_len
#define	ep_p		p_un.ex._ep_p
#define	ep_bbp		p_un.ex._ep_bbp
#define	ep_ebp		p_un.ex._ep_ebp
#define	ep_bxp		p_un.ex._ep_bxp
#define	ep_cnt		p_un.ex._ep_cnt
#define	ep_maxargs	p_un.ex._ep_maxargs
#define	ep_nline	p_un.ex._ep_nline
#define	ep_narg		p_un.ex._ep_narg
#define	ep_rval		p_un.ex._ep_rval
#define	exit_val	p_un._exit_val
#define	max_data	p_un._max_data
#define	min_data	p_un._min_data
#define	regexp_data	p_un._regexp_data
#define	fprint_file	p_un._fprint_file

typedef struct _option {
	const char *name;		/* option name */
	enum ntype token;		/* token type */
	PLAN *(*create)(char ***, int);	/* create function */
	int arg;			/* function needs arg */
} OPTION;

#include "extern.h"
