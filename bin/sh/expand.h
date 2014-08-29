/*	$NetBSD: expand.h,v 1.19 2012/12/22 20:15:22 dsl Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *	@(#)expand.h	8.2 (Berkeley) 5/4/95
 */

#include <inttypes.h>

struct strlist {
	struct strlist *next;
	char *text;
};


struct arglist {
	struct strlist *list;
	struct strlist **lastp;
};

/*
 * expandarg() flags
 */
#define EXP_FULL	0x1	/* perform word splitting & file globbing */
#define EXP_TILDE	0x2	/* do normal tilde expansion */
#define	EXP_VARTILDE	0x4	/* expand tildes in an assignment */
#define	EXP_REDIR	0x8	/* file glob for a redirection (1 match only) */
#define EXP_CASE	0x10	/* keeps quotes around for CASE pattern */
#define EXP_IFS_SPLIT	0x20	/* need to record arguments for ifs breakup */
#define EXP_IN_QUOTES	0x40	/* don't set EXP_IFS_SPLIT again */


union node;
void expandhere(union node *, int);
void expandarg(union node *, struct arglist *, int);
void expari(int);
int patmatch(char *, char *, int);
void rmescapes(char *);
int casematch(union node *, char *);

/* From arith.y */
intmax_t arith(const char *);
void arith_lex_reset(void);
int yylex(void);
