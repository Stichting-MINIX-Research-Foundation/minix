/*	$NetBSD: eval.h,v 1.16 2014/05/31 14:42:18 christos Exp $	*/

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
 *	@(#)eval.h	8.2 (Berkeley) 5/4/95
 */

extern const char *commandname;	/* currently executing command */
extern int exitstatus;		/* exit status of last command */
extern int back_exitstatus;	/* exit status of backquoted command */
extern struct strlist *cmdenviron;  /* environment for builtin command */


struct backcmd {		/* result of evalbackcmd */
	int fd;			/* file descriptor to read from */
	char *buf;		/* buffer */
	int nleft;		/* number of chars in buffer */
	struct job *jp;		/* job structure for command */
};

void evalstring(char *, int);
union node;	/* BLETCH for ansi C */
void evaltree(union node *, int);
void evalbackcmd(union node *, struct backcmd *);

/* in_function returns nonzero if we are currently evaluating a function */
int in_function(void);		/* return non-zero, if evaluating a function */

/* reasons for skipping commands (see comment on breakcmd routine) */
enum skipstate {
      SKIPNONE  = 0,	/* not skipping */
      SKIPBREAK,	/* break */
      SKIPCONT,		/* continue */
      SKIPFUNC,		/* return in a function */
      SKIPFILE		/* return in a dot command */
};

enum skipstate current_skipstate(void);
void stop_skipping(void);	/* reset internal skipping state to SKIPNONE */

/*
 * Only for use by reset() in init.c!
 */
void reset_eval(void);
