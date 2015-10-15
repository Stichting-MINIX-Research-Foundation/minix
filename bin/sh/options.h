/*	$NetBSD: options.h,v 1.22 2015/05/26 21:35:15 christos Exp $	*/

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
 *	@(#)options.h	8.2 (Berkeley) 5/4/95
 */

struct shparam {
	int nparam;		/* # of positional parameters (without $0) */
	unsigned char malloc;	/* if parameter list dynamically allocated */
	unsigned char reset;	/* if getopts has been reset */
	char **p;		/* parameter list */
	char **optnext;		/* next parameter to be processed by getopts */
	char *optptr;		/* used by getopts */
};


struct optent {
	const char *name;		/* for set -o <name> */
	const char letter;		/* set [+/-]<letter> and $- */
	const char opt_set;		/* mutually exclusive option set */
	unsigned char val;		/* value of <letter>flag */
};

/* Those marked [U] are required by posix, but have no effect! */

#ifdef DEFINE_OPTIONS
#define DEF_OPTS(name, letter, opt_set) {name, letter, opt_set, 0},
struct optent optlist[] = {
#else
#define DEF_OPTS(name, letter, opt_set)
#endif
#define DEF_OPT(name,letter) DEF_OPTS(name, letter, 0)

DEF_OPT( "errexit",	'e' )	/* exit on error */
#define eflag optlist[0].val
DEF_OPT( "noglob",	'f' )	/* no pathname expansion */
#define fflag optlist[1].val
DEF_OPT( "ignoreeof",	'I' )	/* do not exit on EOF */
#define Iflag optlist[2].val
DEF_OPT( "interactive",'i' )	/* interactive shell */
#define iflag optlist[3].val
DEF_OPT( "monitor",	'm' )	/* job control */
#define mflag optlist[4].val
DEF_OPT( "noexec",	'n' )	/* [U] do not exec commands */
#define nflag optlist[5].val
DEF_OPT( "stdin",	's' )	/* read from stdin */
#define sflag optlist[6].val
DEF_OPT( "xtrace",	'x' )	/* trace after expansion */
#define xflag optlist[7].val
DEF_OPT( "verbose",	'v' )	/* trace read input */
#define vflag optlist[8].val
DEF_OPTS( "vi",		'V', 'V' )	/* vi style editing */
#define Vflag optlist[9].val
DEF_OPTS( "emacs",	'E', 'V' )	/* emacs style editing */
#define	Eflag optlist[10].val
DEF_OPT( "noclobber",	'C' )	/* do not overwrite files with > */
#define	Cflag optlist[11].val
DEF_OPT( "allexport",	'a' )	/* export all variables */
#define	aflag optlist[12].val
DEF_OPT( "notify",	'b' )	/* [U] report completion of background jobs */
#define	bflag optlist[13].val
DEF_OPT( "nounset",	'u' )	/* error expansion of unset variables */
#define	uflag optlist[14].val
DEF_OPT( "quietprofile", 'q' )
#define	qflag optlist[15].val
DEF_OPT( "nolog",	0 )	/* [U] no functon defs in command history */
#define	nolog optlist[16].val
DEF_OPT( "cdprint",	0 )	/* always print result of cd */
#define	cdprint optlist[17].val
DEF_OPT( "tabcomplete",	0 )	/* <tab> causes filename expansion */
#define	tabcomplete optlist[18].val
DEF_OPT( "fork",	'F' )	/* use fork(2) instead of vfork(2) */
#define	usefork optlist[19].val
DEF_OPT( "nopriv",	'p' )	/* preserve privs even if set{u,g}id */
#define pflag optlist[20].val
#ifdef DEBUG
DEF_OPT( "debug",	0 )	/* enable debug prints */
#define	debug optlist[21].val
#endif

#ifdef DEFINE_OPTIONS
	{ 0, 0, 0, 0 },
};
#define NOPTS (sizeof optlist / sizeof optlist[0] - 1)
int sizeof_optlist = sizeof optlist;
#else
extern struct optent optlist[];
extern int sizeof_optlist;
#endif


extern char *minusc;		/* argument to -c option */
extern char *arg0;		/* $0 */
extern struct shparam shellparam;  /* $@ */
extern char **argptr;		/* argument list for builtin commands */
extern char *optionarg;		/* set by nextopt */
extern char *optptr;		/* used by nextopt */

void procargs(int, char **);
void optschanged(void);
void setparam(char **);
void freeparam(volatile struct shparam *);
int nextopt(const char *);
void getoptsreset(const char *);
