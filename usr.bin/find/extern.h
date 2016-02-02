/*	$NetBSD: extern.h,v 1.28 2007/07/19 07:49:30 daniel Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	from: @(#)extern.h	8.3 (Berkeley) 4/16/94
 */

#include <sys/cdefs.h>

void	 brace_subst(char *, char **, char *, int *);
PLAN	*find_create(char ***);
int	 find_execute(PLAN *, char **);
PLAN	*find_formplan(char **);
int	 find_traverse(PLAN *, int (*)(PLAN *, void *), void *);
int	 f_expr(PLAN *, FTSENT *);
PLAN	*not_squish(PLAN *);
PLAN	*or_squish(PLAN *);
PLAN	*paren_squish(PLAN *);
int	 plan_cleanup(PLAN *, void *);
void	 printlong(char *, char *, struct stat *);
int	 queryuser(char **);
void	 show_path(int);

PLAN	*c_amin(char ***, int);
PLAN	*c_anewer(char ***, int);
PLAN	*c_atime(char ***, int);
PLAN	*c_cmin(char ***, int);
PLAN	*c_cnewer(char ***, int);
PLAN	*c_ctime(char ***, int);
PLAN	*c_delete(char ***, int);
PLAN	*c_depth(char ***, int);
PLAN	*c_empty(char ***, int);
PLAN	*c_exec(char ***, int);
PLAN	*c_execdir(char ***, int);
PLAN	*c_exit(char ***, int);
PLAN	*c_false(char ***, int);
PLAN	*c_flags(char ***, int);
PLAN	*c_follow(char ***, int);
PLAN	*c_fprint(char ***, int);
PLAN	*c_fstype(char ***, int);
PLAN	*c_group(char ***, int);
PLAN	*c_iname(char ***, int);
PLAN	*c_inum(char ***, int);
PLAN	*c_iregex(char ***, int);
PLAN	*c_links(char ***, int);
PLAN	*c_ls(char ***, int);
PLAN	*c_maxdepth(char ***, int);
PLAN	*c_mindepth(char ***, int);
PLAN	*c_mmin(char ***, int);
PLAN	*c_mtime(char ***, int);
PLAN	*c_name(char ***, int);
PLAN	*c_newer(char ***, int);
PLAN	*c_nogroup(char ***, int);
PLAN	*c_nouser(char ***, int);
PLAN	*c_path(char ***, int);
PLAN	*c_perm(char ***, int);
PLAN	*c_print(char ***, int);
PLAN	*c_print0(char ***, int);
PLAN	*c_printx(char ***, int);
PLAN	*c_prune(char ***, int);
PLAN	*c_regex(char ***, int);
PLAN	*c_size(char ***, int);
PLAN	*c_type(char ***, int);
PLAN	*c_user(char ***, int);
PLAN	*c_xdev(char ***, int);
PLAN	*c_openparen(char ***, int);
PLAN	*c_closeparen(char ***, int);
PLAN	*c_not(char ***, int);
PLAN	*c_or(char ***, int);
PLAN	*c_null(char ***, int);

extern int ftsoptions, isdeprecated, isdepth, isoutput, issort, isxargs,
	regcomp_flags;
