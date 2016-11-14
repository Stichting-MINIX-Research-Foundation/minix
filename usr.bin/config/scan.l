%{
/*	$NetBSD: scan.l,v 1.25 2015/09/04 10:16:35 uebayasi Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	from: @(#)scan.l	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: scan.l,v 1.25 2015/09/04 10:16:35 uebayasi Exp $");

#include <sys/param.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <ctype.h>
#include <util.h>
#undef ECHO
#include "defs.h"
#include "gram.h"

int	yyline;
const char *yyfile;
const char *lastfile;
char curinclpath[PATH_MAX];
int ifdefstate = -1;
int st;
#define IDS_PARENT_DISABLED \
    ((ifdefstate > 6) && ((((ifdefstate/6)-1) & 1) == 1))
#define IDS_MAX_DEPTH		362797056 /* 6^11 */
/* States for ifdefstate:

  0  -> matched ifdef
  1  -> unmatched ifdef
  2  -> matched elifdef
  3  -> unmatched elifdef
  4  -> matched else
  5  -> unmatched else

  Upon "ifdef", add one and multiply by 6.
  Upon "endif", divide by 6, remove 1.

  ifdef -> MATCH => continue
           MISMATCH => set to 1
  elifdef -> if (!1) -> MISMATCH
             MATCH => set to 2
             MISMATCH => if (2 || 3) set to 3, else set to 1
  else -> if (1) -> MATCH
          MATCH => set to 4
          MISMATCH => set to 5

  in each case, if parent & 1 == 1, MISMATCH
*/
 
/*
 * Data for returning to previous files from include files.
 */
struct incl {
	struct	incl *in_prev;	/* previous includes in effect, if any */
	YY_BUFFER_STATE in_buf;	/* previous lex state */
	const char *in_fname;	/* previous file name */
	int	in_lineno;	/* previous line number */
	int	in_ateof;	/* token to insert at EOF */
	int	in_interesting;	/* previous value for "interesting" */
	int	in_ifdefstate;	/* conditional level */
};
static struct incl *incl;
static int endinclude(void);
static int getincludepath(void);
static int getcurifdef(void);


%}

%option  noyywrap nounput noinput

PATH	[A-Za-z_0-9]*[./][-A-Za-z_0-9./]*
QCHARS	\"(\\.|[^\\"])*\" 
WORD	[A-Za-z_][-A-Za-z_0-9]*
FILENAME	({PATH}|{QCHARS})
RESTOFLINE	[ \t]*(#[^\n]*)?\n

%x	IGNORED

%%
		/* Local variables for yylex() */
		int tok;

and		return AND;
at		return AT;
attach		return ATTACH;
block		return BLOCK;
build		return BUILD;
char		return CHAR;
compile-with	return COMPILE_WITH;
config		return CONFIG;
deffs		return DEFFS;
define		return DEFINE;
defflag		return DEFFLAG;
defopt		return DEFOPT;
defparam	return DEFPARAM;
defpseudo	return DEFPSEUDO;
defpseudodev	return DEFPSEUDODEV;
devclass	return DEVCLASS;
device		return DEVICE;
device-major	return DEVICE_MAJOR;
dumps		return DUMPS;
file		return XFILE;
file-system	return FILE_SYSTEM;
flags		return FLAGS;
ident		return IDENT;
ioconf		return IOCONF;
linkzero	return LINKZERO;
machine		return XMACHINE;
major		return MAJOR;
makeoptions	return MAKEOPTIONS;
maxpartitions	return MAXPARTITIONS;
maxusers	return MAXUSERS;
minor		return MINOR;
needs-count	return NEEDS_COUNT;
needs-flag	return NEEDS_FLAG;
no		return NO;
object		return XOBJECT;
obsolete	return OBSOLETE;
on		return ON;
options		return OPTIONS;
prefix		return PREFIX;
buildprefix	return BUILDPREFIX;
pseudo-device	return PSEUDO_DEVICE;
pseudo-root	return PSEUDO_ROOT;
root		return ROOT;
select		return SELECT;
single		return SINGLE;
source		return SOURCE;
type		return TYPE;
vector 		return VECTOR;
version 	return VERSION;
with		return WITH;

\+=		return PLUSEQ;
:=		return COLONEQ;

<*>ifdef[ \t]+{WORD}{RESTOFLINE} {
		ifdefstate = (ifdefstate + 1) * 6;
		if (ifdefstate >= IDS_MAX_DEPTH) {
			yyerror("too many levels of conditional");
		}
		if (!IDS_PARENT_DISABLED && getcurifdef()) {
			BEGIN(INITIAL);
		} else {
			ifdefstate++;
			BEGIN(IGNORED);
		}
		yyline++;
	}

<*>ifndef[ \t]+{WORD}{RESTOFLINE} {
		ifdefstate = (ifdefstate + 1) * 6;
		if (ifdefstate >= IDS_MAX_DEPTH) {
			yyerror("too many levels of conditional");
		}
		if (!IDS_PARENT_DISABLED && !getcurifdef()) {
			BEGIN(INITIAL);
		} else {
			ifdefstate++;
			BEGIN(IGNORED);
		}
		yyline++;
	}


<*>elifdef[ \t]+{WORD}{RESTOFLINE} {
		st = ifdefstate % 6;
		if (ifdefstate < 0 || st > 3) {
			yyerror("mismatched elifdef");
		}
		if (IDS_PARENT_DISABLED ||
		    st != 1 || !getcurifdef()) {
			if (st == 2 || st == 3) {
				ifdefstate += 3 - st;
			} else {
				ifdefstate += 1 - st;
			}
			BEGIN(IGNORED);
		} else {
			ifdefstate++;
			BEGIN(INITIAL);
		}
		yyline++;
	}

<*>elifndef[ \t]+{WORD}{RESTOFLINE} {
		st = ifdefstate % 6;
		if (ifdefstate < 0 || st > 3) {
			yyerror("mismatched elifndef");
		}
		if (IDS_PARENT_DISABLED ||
		    st != 1 || getcurifdef()) {
			if (st == 2 || st == 3) {
				ifdefstate += 3 - st;
			} else {
				ifdefstate += 1 - st;
			}
			BEGIN(IGNORED);
		} else {
			ifdefstate++;
			BEGIN(INITIAL);
		}
		yyline++;
	}

<*>else{RESTOFLINE} {
		st = ifdefstate % 6;
		if (ifdefstate < 0 || st > 3) {
			yyerror("mismatched else");
		}
		if (!IDS_PARENT_DISABLED && (st == 1)) {
			ifdefstate += 3;
			BEGIN(INITIAL);
		} else {
			ifdefstate += 5 - st;
			BEGIN(IGNORED);
		}
		yyline++;
	}

<*>endif{RESTOFLINE} {
		if (ifdefstate < 0) {
			yyerror("mismatched endif");
		}
		if (!IDS_PARENT_DISABLED) {
			BEGIN(INITIAL);
		}
		ifdefstate = (ifdefstate/6) - 1;
		yyline++;
	}

<IGNORED>\n		{
		yyline++;
	}

<IGNORED>.	/* ignore */

include[ \t]+{FILENAME}{RESTOFLINE}	{
		yyline++;
		if (getincludepath()) {
			include(curinclpath, 0, 0, 1);
		} else {
			yyerror("bad include path-name");
		}
	}

cinclude[ \t]+{FILENAME}{RESTOFLINE}	{
		yyline++;
		if (getincludepath()) {
			include(curinclpath, 0, 1, 1);
		} else {
			yyerror("bad cinclude path-name");
		}
	}

package[ \t]+{FILENAME}{RESTOFLINE}	{
		yyline++;
		if (!oktopackage) {
			yyerror("package not allowed here");
		} else if (getincludepath()) {
			package(curinclpath);
		} else {
			yyerror("bad package path-name");
		}
	}

{PATH}	{
		yylval.str = intern(yytext);
		return PATHNAME;
	}

{WORD}	{
		yylval.str = intern(yytext);
		return WORD;
	}

\"\" {
		yylval.str = intern("");
		return EMPTYSTRING;
	}

{QCHARS}	{
		size_t l = strlen(yytext);
		if (l > 1 && yytext[l - 1] == '"')
			yytext[l - 1] = '\0';

		yylval.str = intern(yytext + 1);
		return QSTRING;
	}
0[0-7]*	{
		yylval.num.fmt = 8;
		yylval.num.val = strtoll(yytext, NULL, 8);
		return NUMBER;
	}
0[xX][0-9a-fA-F]+ {
		yylval.num.fmt = 16;
		yylval.num.val = (long long)strtoull(yytext + 2, NULL, 16);
		return NUMBER;
	}
[1-9][0-9]* {
		yylval.num.fmt = 10;
		yylval.num.val = strtoll(yytext, NULL, 10);
		return NUMBER;
	}
\n[ \t] {
		/*
		 * Note: newline followed by whitespace is always a
		 * continuation of the previous line, so do NOT
		 * return a token in this case.
		 */
		yyline++;
	}
\n	{
		yyline++;
		return '\n';
	}
\00	{
		/* Detect NUL characters in the config file and
		 * error out.
		 */
		cfgerror("NUL character detected at line %i", yyline);
	}
#.*	{ /* ignored (comment) */; }
[ \t]+	{ /* ignored (white space) */; }
.	{ return yytext[0]; }
<*><<EOF>> {
		if (ifdefstate > (incl == NULL ? -1 : incl->in_ifdefstate)) {
			yyerror("reached EOF while looking for endif");
		}
		if (incl == NULL)
			return YY_NULL;
		tok = endinclude();
		if (tok)
			return tok;
		/* otherwise continue scanning */
	}

%%

int interesting = 1;

static int
curdir_push(const char *fname)
{
	struct prefix *pf;
	char *p, *d, *f;

	/* Set up the initial "current directory" for include directives. */
	d = dirname(f = estrdup(fname));
	if (*d == '/')
		p = estrdup(d);
	else {
		char *cwd, buf[PATH_MAX];

		if ((cwd = getcwd(buf, sizeof(buf))) == NULL) {
			free(f);
			return (-1);
		}
		easprintf(&p, "%s/%s", cwd, d);
	}
	free(f);
	pf = ecalloc(1, sizeof(*pf));
	pf->pf_prefix = p;
	SLIST_INSERT_HEAD(&curdirs, pf, pf_next);

	return (0);
}

static void
curdir_pop(void)
{
	struct prefix *pf;

	pf = SLIST_FIRST(&curdirs);
	SLIST_REMOVE_HEAD(&curdirs, pf_next);
	if (SLIST_EMPTY(&curdirs))
		panic("curdirs is empty");
	/* LINTED cast away const (pf_prefix is malloc'd for curdirs) */
	free((void *)__UNCONST(pf->pf_prefix));
	free(pf);
}

/*
 * Open the "main" file (conffile).
 */
int
firstfile(const char *fname)
{

#if defined(__NetBSD__)
	if ((yyin = fopen(fname, "rf")) == NULL)
#else
	if ((yyin = fopen(fname, "r")) == NULL)
#endif
		return (-1);

	if (curdir_push(fname) == -1)
		return (-1);

	yyfile = conffile = fname;
	yyline = 1;
	return (0);
}

/*
 * Add a "package" to the configuration.  This is essentially
 * syntactic sugar around the sequence:
 *
 *	prefix ../some/directory
 *	include "files.package"
 *	prefix
 */
void
package(const char *fname)
{
	char *fname1 = estrdup(fname);
	char *fname2 = estrdup(fname);
	char *dir = dirname(fname1);
	char *file = basename(fname2);

	/*
	 * Push the prefix on to the prefix stack and process the include
	 * file.  When we reach the end of the include file, inserting
	 * the PREFIX token into the input stream will pop the prefix off
	 * of the prefix stack.
	 */
	prefix_push(dir);
	(void) include(file, PREFIX, 0, 1);

	free(fname1);
	free(fname2);
}

int includedepth;

/*
 * Open the named file for inclusion at the current point.  Returns 0 on
 * success (file opened and previous state pushed), nonzero on failure
 * (fopen failed, complaint made).  The `ateof' parameter controls the
 * token to be inserted at the end of the include file (i.e. ENDFILE).
 * If ateof == 0 then nothing is inserted.
 */
int
include(const char *fname, int ateof, int conditional, int direct)
{
	FILE *fp;
	struct incl *in;
	char *s;
	static int havedirs;
	extern int vflag;

	if (havedirs == 0) {
		havedirs = 1;
		setupdirs();
	}

	if (fname[0] == '/')
		s = estrdup(fname);
	else if (fname[0] == '.' && fname[1] == '/') {
		struct prefix *pf = SLIST_FIRST(&curdirs);
		easprintf(&s, "%s/%s", pf->pf_prefix, fname + 2);
	} else
		s = sourcepath(fname);
	if ((fp = fopen(s, "r")) == NULL) {
		if (conditional == 0)
			cfgerror("cannot open %s for reading: %s", s,
			    strerror(errno));
		else if (vflag)
			cfgwarn("cannot open conditional include file %s: %s",
			     s, strerror(errno));
		free(s);
		return (-1);
	}
	if (curdir_push(s) == -1) {
		cfgerror("cannot record current working directory for %s", s);
		fclose(fp);
		free(s);
		return (-1);
	}
	in = ecalloc(1, sizeof *in);
	in->in_prev = incl;
	in->in_buf = YY_CURRENT_BUFFER;
	in->in_fname = yyfile;
	in->in_lineno = yyline;
	in->in_ateof = ateof;
	in->in_interesting = interesting;
	in->in_ifdefstate = ifdefstate;
	interesting = direct & interesting;
	if (interesting)
		logconfig_include(fp, fname);
	incl = in;
	CFGDBG(1, "include `%s' from `%s' line %d", fname, yyfile, yyline);
	yy_switch_to_buffer(yy_create_buffer(fp, YY_BUF_SIZE));
	yyfile = intern(s);
	yyline = 1;
	free(s);
	includedepth++;
	return (0);
}

/*
 * Extract the pathname from a include/cinclude/package into curinclpath
 */
static int
getincludepath(void)
{
	const char *p = yytext;
	ptrdiff_t len;
	const char *e;

	while (*p && isascii((unsigned int)*p) && !isspace((unsigned int)*p))
		p++;
	while (*p && isascii((unsigned int)*p) && isspace((unsigned int)*p))
		p++;
	if (!*p)
		return 0;
	if (*p == '"') {
		p++;
		e = strchr(p, '"');
		if (!e) return 0;
	} else {
		e = p;
		while (*e && isascii((unsigned int)*e)
		    && !isspace((unsigned int)*e))
			e++;
	}

	len = e-p;
	if (len > (ptrdiff_t)sizeof(curinclpath)-1)
		len = sizeof(curinclpath)-1;
	strncpy(curinclpath, p, sizeof(curinclpath));
	curinclpath[len] = '\0';

	return 1;
}

/*
 * Terminate the most recent inclusion.
 */
static int
endinclude(void)
{
	struct incl *in;
	int ateof;

	curdir_pop();
	if ((in = incl) == NULL)
		panic("endinclude");
	incl = in->in_prev;
	lastfile = yyfile;
	yy_delete_buffer(YY_CURRENT_BUFFER);
	(void)fclose(yyin);
	yy_switch_to_buffer(in->in_buf);
	yyfile = in->in_fname;
	yyline = in->in_lineno;
	ateof  = in->in_ateof;
	interesting = in->in_interesting;
	free(in);

	includedepth--;

	return (ateof);
}

/*
 * Return the current line number.  If yacc has looked ahead and caused
 * us to consume a newline, we have to subtract one.  yychar is yacc's
 * token lookahead, so we can tell.
 */
u_short
currentline(void)
{
	extern int yychar;

	return (u_short)(yyline - (yychar == '\n'));
}

static int
getcurifdef(void)
{
	char *p = yytext, *q;

	while (*p && isascii((unsigned int)*p) && !isspace((unsigned int)*p))
		p++;
	while (*p && isascii((unsigned int)*p) && isspace((unsigned int)*p))
		p++;
	q = p;
	while (*q && isascii((unsigned int)*q) && !isspace((unsigned int)*q))
		q++;
	*q = '\0';

	return ht_lookup(attrtab, intern(p)) != NULL;
}
