/* $NetBSD: lex.c,v 1.31 2013/08/06 05:42:43 christos Exp $ */

/*-
 * Copyright (c) 1980, 1991, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)lex.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: lex.c,v 1.31 2013/08/06 05:42:43 christos Exp $");
#endif
#endif /* not lint */

#include <sys/ioctl.h>
#include <sys/types.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "csh.h"
#include "extern.h"

/*
 * These lexical routines read input and form lists of words.
 * There is some involved processing here, because of the complications
 * of input buffering, and especially because of history substitution.
 */

static Char *word(void);
static int getC1(int);
static void getdol(void);
static void getexcl(int);
static struct Hist *findev(Char *, int);
static void setexclp(Char *);
static int bgetc(void);
static void bfree(void);
static struct wordent *gethent(int);
static int matchs(Char *, Char *);
static int getsel(int *, int *, int);
static struct wordent *getsub(struct wordent *);
static Char *subword(Char *, int, int *);
static struct wordent *dosub(int, struct wordent *, int);

/*
 * Peekc is a peek character for getC, peekread for readc.
 * There is a subtlety here in many places... history routines
 * will read ahead and then insert stuff into the input stream.
 * If they push back a character then they must push it behind
 * the text substituted by the history substitution.  On the other
 * hand in several places we need 2 peek characters.  To make this
 * all work, the history routines read with getC, and make use both
 * of ungetC and unreadc.  The key observation is that the state
 * of getC at the call of a history reference is such that calls
 * to getC from the history routines will always yield calls of
 * readc, unless this peeking is involved.  That is to say that during
 * getexcl the variables lap, exclp, and exclnxt are all zero.
 *
 * Getdol invokes history substitution, hence the extra peek, peekd,
 * which it can ungetD to be before history substitutions.
 */
static int peekc = 0, peekd = 0;
static int peekread = 0;

/* (Tail of) current word from ! subst */
static Char *exclp = NULL;

/* The rest of the ! subst words */
static struct wordent *exclnxt = NULL;

/* Count of remaining words in ! subst */
static int exclc = 0;

/* "Globp" for alias resubstitution */
Char **alvec, *alvecp;
int aret = F_SEEK;

/*
 * Labuf implements a general buffer for lookahead during lexical operations.
 * Text which is to be placed in the input stream can be stuck here.
 * We stick parsed ahead $ constructs during initial input,
 * process id's from `$$', and modified variable values (from qualifiers
 * during expansion in sh.dol.c) here.
 */
static Char labuf[BUFSIZE];

/*
 * Lex returns to its caller not only a wordlist (as a "var" parameter)
 * but also whether a history substitution occurred.  This is used in
 * the main (process) routine to determine whether to echo, and also
 * when called by the alias routine to determine whether to keep the
 * argument list.
 */
static int hadhist = 0;

/*
 * Avoid alias expansion recursion via \!#
 */
int     hleft;

static int getCtmp;

#define getC(f) ((getCtmp = peekc) ? (peekc = 0, getCtmp) : getC1(f))
#define	ungetC(c) peekc = c
#define	ungetD(c) peekd = c

int
lex(struct wordent *hp)
{
    struct wordent *wdp;
    int c;

    btell(&lineloc);
    hp->next = hp->prev = hp;
    hp->word = STRNULL;
    hadhist = 0;
    do
	c = readc(0);
    while (c == ' ' || c == '\t');
    if (c == HISTSUB && intty)
	/* ^lef^rit	from tty is short !:s^lef^rit */
	getexcl(c);
    else
	unreadc(c);
    wdp = hp;
    /*
     * The following loop is written so that the links needed by freelex will
     * be ready and rarin to go even if it is interrupted.
     */
    do {
	struct wordent *new;

	new = xmalloc(sizeof(*wdp));
	new->word = 0;
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	wdp->word = word();
    } while (wdp->word[0] != '\n');
    hp->prev = wdp;
    return (hadhist);
}

void
prlex(FILE *fp, struct wordent *sp0)
{
    struct wordent *sp;

    sp = sp0->next;
    for (;;) {
	(void)fprintf(fp, "%s", vis_str(sp->word));
	sp = sp->next;
	if (sp == sp0)
	    break;
	if (sp->word[0] != '\n')
	    (void) fputc(' ', fp);
    }
}

#ifdef EDIT
int
sprlex(char **s, struct wordent *sp0)
{
    struct wordent *sp;

    sp = sp0->next;
    char *os = *s;
    for (;;) {
	char *w = vis_str(sp->word);
	if (os == NULL) {
	    if (asprintf(s, "%s", w) < 0)
		return -1;
	    os = *s;
	} else if (*os != '\n') {
	    if (asprintf(s, "%s %s", os, w) < 0) {
		free(os);
		return 1;
	    }
	    free(os);
	    os = *s;
	}
	sp = sp->next;
	if (sp == sp0)
	    break;
    }
    return 0;
}
#endif

void
copylex(struct wordent *hp, struct wordent *fp)
{
    struct wordent *wdp;

    wdp = hp;
    fp = fp->next;
    do {
	struct wordent *new;

	new = xmalloc(sizeof(*wdp));
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	wdp->word = Strsave(fp->word);
	fp = fp->next;
    } while (wdp->word[0] != '\n');
    hp->prev = wdp;
}

void
freelex(struct wordent *vp)
{
    struct wordent *fp;

    while (vp->next != vp) {
	fp = vp->next;
	vp->next = fp->next;
	xfree((ptr_t) fp->word);
	xfree((ptr_t) fp);
    }
    vp->prev = vp;
}

static Char *
word(void)
{
    Char wbuf[BUFSIZE], *wp;
    int i, c, c1;
    int dolflg;

    wp = wbuf;
    i = BUFSIZE - 4;
loop:
    while ((c = getC(DOALL)) == ' ' || c == '\t')
	continue;
    if (cmap(c, _META | _ESC))
	switch (c) {
	case '&':
	case '|':
	case '<':
	case '>':
	    *wp++ = (Char)c;
	    c1 = getC(DOALL);
	    if (c1 == c)
		*wp++ = (Char)c1;
	    else
		ungetC(c1);
	    goto ret;

	case '#':
	    if (intty)
		break;
	    c = 0;
	    do {
		c1 = c;
		c = getC(0);
	    } while (c != '\n');
	    if (c1 == '\\')
		goto loop;
	    /* FALLTHROUGH */

	case ';':
	case '(':
	case ')':
	case '\n':
	    *wp++ = (Char)c;
	    goto ret;

	case '\\':
	    c = getC(0);
	    if (c == '\n') {
		if (onelflg == 1)
		    onelflg = 2;
		goto loop;
	    }
	    if (c != HIST)
		*wp++ = '\\', --i;
	    c |= QUOTE;
	    break;
	}
    c1 = 0;
    dolflg = DOALL;
    for (;;) {
	if (c1) {
	    if (c == c1) {
		c1 = 0;
		dolflg = DOALL;
	    }
	    else if (c == '\\') {
		c = getC(0);
		if (c == HIST)
		    c |= QUOTE;
		else {
		    if (c == '\n')
			/*
			 * if (c1 == '`') c = ' '; else
			 */
			c |= QUOTE;
		    ungetC(c);
		    c = '\\';
		}
	    }
	    else if (c == '\n') {
		seterror(ERR_UNMATCHED, c1);
		ungetC(c);
		break;
	    }
	}
	else if (cmap(c, _META | _QF | _QB | _ESC)) {
	    if (c == '\\') {
		c = getC(0);
		if (c == '\n') {
		    if (onelflg == 1)
			onelflg = 2;
		    break;
		}
		if (c != HIST)
		    *wp++ = '\\', --i;
		c |= QUOTE;
	    }
	    else if (cmap(c, _QF | _QB)) {	/* '"` */
		c1 = c;
		dolflg = c == '"' ? DOALL : DOEXCL;
	    }
	    else if (c != '#' || !intty) {
		ungetC(c);
		break;
	    }
	}
	if (--i > 0) {
	    *wp++ = (Char)c;
	    c = getC(dolflg);
	}
	else {
	    seterror(ERR_WTOOLONG);
	    wp = &wbuf[1];
	    break;
	}
    }
ret:
    *wp = 0;
    return (Strsave(wbuf));
}

static int
getC1(int flag)
{
    int c;

    for (;;) {
	if ((c = peekc) != '\0') {
	    peekc = 0;
	    return (c);
	}
	if (lap) {
	    if ((c = *lap++) == 0)
		lap = 0;
	    else {
		if (cmap(c, _META | _QF | _QB))
		    c |= QUOTE;
		return (c);
	    }
	}
	if ((c = peekd) != '\0') {
	    peekd = 0;
	    return (c);
	}
	if (exclp) {
	    if ((c = *exclp++) != '\0')
		return (c);
	    if (exclnxt && --exclc >= 0) {
		exclnxt = exclnxt->next;
		setexclp(exclnxt->word);
		return (' ');
	    }
	    exclp = 0;
	    exclnxt = 0;
	}
	if (exclnxt) {
	    exclnxt = exclnxt->next;
	    if (--exclc < 0)
		exclnxt = 0;
	    else
		setexclp(exclnxt->word);
	    continue;
	}
	c = readc(0);
	if (c == '$' && (flag & DODOL)) {
	    getdol();
	    continue;
	}
	if (c == HIST && (flag & DOEXCL)) {
	    getexcl(0);
	    continue;
	}
	break;
    }
    return (c);
}

static void
getdol(void)
{
    Char name[4*MAXVARLEN+1], *ep, *np;
    int c, sc;
    int special, toolong;

    special = 0;
    np = name, *np++ = '$';
    c = sc = getC(DOEXCL);
    if (any("\t \n", c)) {
	ungetD(c);
	ungetC('$' | QUOTE);
	return;
    }
    if (c == '{')
	*np++ = (Char)c, c = getC(DOEXCL);
    if (c == '#' || c == '?')
	special++, *np++ = (Char)c, c = getC(DOEXCL);
    *np++ = (Char)c;
    switch (c) {
    case '<':
    case '$':
    case '!':
	if (special)
	    seterror(ERR_SPDOLLT);
	*np = 0;
	addla(name);
	return;
    case '\n':
	ungetD(c);
	np--;
	seterror(ERR_NEWLINE);
	*np = 0;
	addla(name);
	return;
    case '*':
	if (special)
	    seterror(ERR_SPSTAR);
	*np = 0;
	addla(name);
	return;
    default:
	toolong = 0;
	if (Isdigit(c)) {
#ifdef notdef
	    /* let $?0 pass for now */
	    if (special) {
		seterror(ERR_DIGIT);
		*np = 0;
		addla(name);
		return;
	    }
#endif
	    /* we know that np < &name[4] */
	    ep = &np[MAXVARLEN];
	    while ((c = getC(DOEXCL)) != '\0'){
		if (!Isdigit(c))
		    break;
		if (np < ep)
		    *np++ = (Char)c;
		else
		    toolong = 1;
	    }
	}
	else if (letter(c)) {
	    /* we know that np < &name[4] */
	    ep = &np[MAXVARLEN];
	    toolong = 0;
	    while ((c = getC(DOEXCL)) != '\0') {
		/* Bugfix for ${v123x} from Chris Torek, DAS DEC-90. */
		if (!letter(c) && !Isdigit(c))
		    break;
		if (np < ep)
		    *np++ = (Char)c;
		else
		    toolong = 1;
	    }
	}
	else {
	    *np = 0;
	    seterror(ERR_VARILL);
	    addla(name);
	    return;
	}
	if (toolong) {
	    seterror(ERR_VARTOOLONG);
	    *np = 0;
	    addla(name);
	    return;
	}
	break;
    }
    if (c == '[') {
	*np++ = (Char)c;
	/*
	 * Name up to here is a max of MAXVARLEN + 8.
	 */
	ep = &np[2 * MAXVARLEN + 8];
	do {
	    /*
	     * Michael Greim: Allow $ expansion to take place in selector
	     * expressions. (limits the number of characters returned)
	     */
	    c = getC(DOEXCL | DODOL);
	    if (c == '\n') {
		ungetD(c);
		np--;
		seterror(ERR_NLINDEX);
		*np = 0;
		addla(name);
		return;
	    }
	    if (np < ep)
		*np++ = (Char)c;
	} while (c != ']');
	*np = '\0';
	if (np >= ep) {
	    seterror(ERR_SELOVFL);
	    addla(name);
	    return;
	}
	c = getC(DOEXCL);
    }
    /*
     * Name up to here is a max of 2 * MAXVARLEN + 8.
     */
    if (c == ':') {
	/*
	 * if the :g modifier is followed by a newline, then error right away!
	 * -strike
	 */
	int amodflag, gmodflag;

	amodflag = 0;
	gmodflag = 0;
	do {
	    *np++ = (Char)c, c = getC(DOEXCL);
	    if (c == 'g' || c == 'a') {
		if (c == 'g')
		    gmodflag++;
		else
		    amodflag++;
		*np++ = (Char)c; c = getC(DOEXCL);
	    }
	    if ((c == 'g' && !gmodflag) || (c == 'a' && !amodflag)) {
		if (c == 'g')
		    gmodflag++;
		else
		    amodflag++;
		*np++ = (Char)c, c = getC(DOEXCL);
	    }
	    *np++ = (Char)c;
	    /* scan s// [eichin:19910926.0512EST] */
	    if (c == 's') {
		int delimcnt = 2;
		int delim = getC(0);
		*np++ = (Char)delim;
		
		if (!delim || letter(delim)
		    || Isdigit(delim) || any(" \t\n", delim)) {
		    seterror(ERR_BADSUBST);
		    break;
		}	
		while ((c = getC(0)) != -1) {
		    *np++ = (Char)c;
		    if(c == delim) delimcnt--;
		    if(!delimcnt) break;
		}
		if(delimcnt) {
		    seterror(ERR_BADSUBST);
		    break;
		}
		c = 's';
	    }
	    if (!any("htrqxes", c)) {
		if ((amodflag || gmodflag) && c == '\n')
		    stderror(ERR_VARSYN);	/* strike */
		seterror(ERR_VARMOD, c);
		*np = 0;
		addla(name);
		return;
	    }
	}
	while ((c = getC(DOEXCL)) == ':');
	ungetD(c);
    }
    else
	ungetD(c);
    if (sc == '{') {
	c = getC(DOEXCL);
	if (c != '}') {
	    ungetD(c);
	    seterror(ERR_MISSING, '}');
	    *np = 0;
	    addla(name);
	    return;
	}
	*np++ = (Char)c;
    }
    *np = 0;
    addla(name);
    return;
}

void
addla(Char *cp)
{
    Char buf[BUFSIZE];

    if (Strlen(cp) + (lap ? Strlen(lap) : 0) >=
	(sizeof(labuf) - 4) / sizeof(Char)) {
	seterror(ERR_EXPOVFL);
	return;
    }
    if (lap)
	(void)Strcpy(buf, lap);
    (void)Strcpy(labuf, cp);
    if (lap)
	(void)Strcat(labuf, buf);
    lap = labuf;
}

static Char lhsb[32];
static Char slhs[32];
static Char rhsb[64];
static int quesarg;

static void
getexcl(int sc)
{
    struct wordent *hp, *ip;
    int c, dol, left, right;

    if (sc == 0) {
	sc = getC(0);
	if (sc != '{') {
	    ungetC(sc);
	    sc = 0;
	}
    }
    quesarg = -1;
    lastev = eventno;
    hp = gethent(sc);
    if (hp == 0)
	return;
    hadhist = 1;
    dol = 0;
    if (hp == alhistp)
	for (ip = hp->next->next; ip != alhistt; ip = ip->next)
	    dol++;
    else
	for (ip = hp->next->next; ip != hp->prev; ip = ip->next)
	    dol++;
    left = 0, right = dol;
    if (sc == HISTSUB) {
	ungetC('s'), unreadc(HISTSUB), c = ':';
	goto subst;
    }
    c = getC(0);
    if (!any(":^$*-%", c))
	goto subst;
    left = right = -1;
    if (c == ':') {
	c = getC(0);
	unreadc(c);
	if (letter(c) || c == '&') {
	    c = ':';
	    left = 0, right = dol;
	    goto subst;
	}
    }
    else
	ungetC(c);
    if (!getsel(&left, &right, dol))
	return;
    c = getC(0);
    if (c == '*')
	ungetC(c), c = '-';
    if (c == '-') {
	if (!getsel(&left, &right, dol))
	    return;
	c = getC(0);
    }
subst:
    exclc = right - left + 1;
    while (--left >= 0)
	hp = hp->next;
    if (sc == HISTSUB || c == ':') {
	do {
	    hp = getsub(hp);
	    c = getC(0);
	} while (c == ':');
    }
    unreadc(c);
    if (sc == '{') {
	c = getC(0);
	if (c != '}')
	    seterror(ERR_BADBANG);
    }
    exclnxt = hp;
}

static struct wordent *
getsub(struct wordent *en)
{
    Char orhsb[sizeof(rhsb) / sizeof(Char)];
    Char *cp;
    int c, delim, sc;
    int global;

    do {
	exclnxt = 0;
	global = 0;
	sc = c = getC(0);
	if (c == 'g' || c == 'a') {
	    global |= (c == 'g') ? 1 : 2;
	    sc = c = getC(0);
	}
	if (((c =='g') && !(global & 1)) || ((c == 'a') && !(global & 2))) {
	    global |= (c == 'g') ? 1 : 2;
	    sc = c = getC(0);
	}

	switch (c) {
	case 'p':
	    justpr++;
	    return (en);
	case 'x':
	case 'q':
	    global |= 1;
	    /* FALLTHROUGH */
	case 'h':
	case 'r':
	case 't':
	case 'e':
	    break;
	case '&':
	    if (slhs[0] == 0) {
		seterror(ERR_NOSUBST);
		return (en);
	    }
	    (void) Strcpy(lhsb, slhs);
	    break;
#ifdef notdef
	case '~':
	    if (lhsb[0] == 0)
		goto badlhs;
	    break;
#endif
	case 's':
	    delim = getC(0);
	    if (letter(delim) || Isdigit(delim) || any(" \t\n", delim)) {
		unreadc(delim);
		lhsb[0] = 0;
		seterror(ERR_BADSUBST);
		return (en);
	    }
	    cp = lhsb;
	    for (;;) {
		c = getC(0);
		if (c == '\n') {
		    unreadc(c);
		    break;
		}
		if (c == delim)
		    break;
		if (cp > &lhsb[sizeof(lhsb) / sizeof(Char) - 2]) {
		    lhsb[0] = 0;
		    seterror(ERR_BADSUBST);
		    return (en);
		}
		if (c == '\\') {
		    c = getC(0);
		    if (c != delim && c != '\\')
			*cp++ = '\\';
		}
		*cp++ = (Char)c;
	    }
	    if (cp != lhsb)
		*cp++ = 0;
	    else if (lhsb[0] == 0) {
		seterror(ERR_LHS);
		return (en);
	    }
	    cp = rhsb;
	    (void)Strcpy(orhsb, cp);
	    for (;;) {
		c = getC(0);
		if (c == '\n') {
		    unreadc(c);
		    break;
		}
		if (c == delim)
		    break;
#ifdef notdef
		if (c == '~') {
		    if (&cp[Strlen(orhsb)] > &rhsb[sizeof(rhsb) /
						   sizeof(Char) - 2])
			goto toorhs;
		    (void)Strcpy(cp, orhsb);
		    cp = Strend(cp);
		    continue;
		}
#endif
		if (cp > &rhsb[sizeof(rhsb) / sizeof(Char) - 2]) {
		    seterror(ERR_RHSLONG);
		    return (en);
		}
		if (c == '\\') {
		    c = getC(0);
		    if (c != delim /* && c != '~' */ )
			*cp++ = '\\';
		}
		*cp++ = (Char)c;
	    }
	    *cp++ = 0;
	    break;
	default:
	    if (c == '\n')
		unreadc(c);
	    seterror(ERR_BADBANGMOD, c);
	    return (en);
	}
	(void)Strcpy(slhs, lhsb);
	if (exclc)
	    en = dosub(sc, en, global);
    }
    while ((c = getC(0)) == ':');
    unreadc(c);
    return (en);
}

static struct wordent *
dosub(int sc, struct wordent *en, int global)
{
    struct wordent lexi, *hp, *wdp;
    int i;
    int didone, didsub;

    didone = 0;
    didsub = 0;
    i = exclc;
    hp = &lexi;

    wdp = hp;
    while (--i >= 0) {
	struct wordent *new = (struct wordent *)xcalloc(1, sizeof *wdp);

	new->word = 0;
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	en = en->next;
	if (en->word) {
	    Char *tword, *otword;

	    if ((global & 1) || didsub == 0) {
		tword = subword(en->word, sc, &didone);
		if (didone)
		    didsub = 1;
		if (global & 2) {
		    while (didone && tword != STRNULL) {
			otword = tword;
			tword = subword(otword, sc, &didone);
			if (Strcmp(tword, otword) == 0) {
			    xfree((ptr_t) otword);
			    break;
			}
			else
			    xfree((ptr_t)otword);
		    }
		}
	    }
	    else
		tword = Strsave(en->word);
	    wdp->word = tword;
	}
    }
    if (didsub == 0)
	seterror(ERR_MODFAIL);
    hp->prev = wdp;
    return (&enthist(-1000, &lexi, 0)->Hlex);
}

static Char *
subword(Char *cp, int type, int *adid)
{
    Char wbuf[BUFSIZE];
    Char *mp, *np, *wp;
    ssize_t i;

    *adid = 0;
    switch (type) {
    case 'r':
    case 'e':
    case 'h':
    case 't':
    case 'q':
    case 'x':
	wp = domod(cp, type);
	if (wp == 0)
	    return (Strsave(cp));
	*adid = 1;
	return (wp);
    default:
	wp = wbuf;
	i = BUFSIZE - 4;
	for (mp = cp; *mp; mp++)
	    if (matchs(mp, lhsb)) {
		for (np = cp; np < mp;)
		    *wp++ = *np++, --i;
		for (np = rhsb; *np; np++)
		    switch (*np) {
		    case '\\':
			if (np[1] == '&')
			    np++;
			/* FALLTHROUGH */
		    default:
			if (--i < 0) {
			    seterror(ERR_SUBOVFL);
			    return (STRNULL);
			}
			*wp++ = *np;
			continue;
		    case '&':
			i -= (ssize_t)Strlen(lhsb);
			if (i < 0) {
			    seterror(ERR_SUBOVFL);
			    return (STRNULL);
			}
			*wp = 0;
			(void) Strcat(wp, lhsb);
			wp = Strend(wp);
			continue;
		    }
		mp += Strlen(lhsb);
		i -= (ssize_t)Strlen(mp);
		if (i < 0) {
		    seterror(ERR_SUBOVFL);
		    return (STRNULL);
		}
		*wp = 0;
		(void) Strcat(wp, mp);
		*adid = 1;
		return (Strsave(wbuf));
	    }
	return (Strsave(cp));
    }
}

Char *
domod(Char *cp, int type)
{
    Char *wp, *xp;
    int c;

    switch (type) {
    case 'x':
    case 'q':
	wp = Strsave(cp);
	for (xp = wp; (c = *xp) != '\0'; xp++)
	    if ((c != ' ' && c != '\t') || type == 'q')
		*xp |= QUOTE;
	return (wp);
    case 'h':
    case 't':
	if (!any(short2str(cp), '/'))
	    return (type == 't' ? Strsave(cp) : 0);
	wp = Strend(cp);
	while (*--wp != '/')
	    continue;
	if (type == 'h')
	    xp = Strsave(cp), xp[wp - cp] = 0;
	else
	    xp = Strsave(wp + 1);
	return (xp);
    case 'e':
    case 'r':
	wp = Strend(cp);
	for (wp--; wp >= cp && *wp != '/'; wp--)
	    if (*wp == '.') {
		if (type == 'e')
		    xp = Strsave(wp + 1);
		else
		    xp = Strsave(cp), xp[wp - cp] = 0;
		return (xp);
	    }
	return (Strsave(type == 'e' ? STRNULL : cp));
    default:
	break;
    }
    return (0);
}

static int
matchs(Char *str, Char *pat)
{
    while (*str && *pat && *str == *pat)
	str++, pat++;
    return (*pat == 0);
}

static int
getsel(int *al, int *ar, int dol)
{
    int c, i;
    int first;

    c = getC(0);
    first = *al < 0;

    switch (c) {
    case '%':
	if (quesarg == -1) {
	    seterror(ERR_BADBANGARG);
	    return (0);
	}
	if (*al < 0)
	    *al = quesarg;
	*ar = quesarg;
	break;
    case '-':
	if (*al < 0) {
	    *al = 0;
	    *ar = dol - 1;
	    unreadc(c);
	}
	return (1);
    case '^':
	if (*al < 0)
	    *al = 1;
	*ar = 1;
	break;
    case '$':
	if (*al < 0)
	    *al = dol;
	*ar = dol;
	break;
    case '*':
	if (*al < 0)
	    *al = 1;
	*ar = dol;
	if (*ar < *al) {
	    *ar = 0;
	    *al = 1;
	    return (1);
	}
	break;
    default:
	if (Isdigit(c)) {
	    i = 0;
	    while (Isdigit(c)) {
		i = i * 10 + c - '0';
		c = getC(0);
	    }
	    if (i < 0)
		i = dol + 1;
	    if (*al < 0)
		*al = i;
	    *ar = i;
	}
	else if (*al < 0)
	    *al = 0, *ar = dol;
	else
	    *ar = dol - 1;
	unreadc(c);
	break;
    }
    if (first) {
	c = getC(0);
	unreadc(c);
	if (any("-$*", c))
	    return (1);
    }
    if (*al > *ar || *ar > dol) {
	seterror(ERR_BADBANGARG);
	return (0);
    }
    return (1);

}

static struct wordent *
gethent(int sc)
{
    struct Hist *hp;
    Char *np;
    char *str;
    int c, event;
    int back;

    back = 0;
    c = sc == HISTSUB ? HIST : getC(0);
    if (c == HIST) {
	if (alhistp)
	    return (alhistp);
	event = eventno;
    }
    else
	switch (c) {
	case ':':
	case '^':
	case '$':
	case '*':
	case '%':
	    ungetC(c);
	    if (lastev == eventno && alhistp)
		return (alhistp);
	    event = lastev;
	    break;
	case '#':		/* !# is command being typed in (mrh) */
	    if (--hleft == 0) {
		seterror(ERR_HISTLOOP);
		return (0);
	    }
	    else
		return (&paraml);
	    /* NOTREACHED */
	case '-':
	    back = 1;
	    c = getC(0);
	    /* FALLTHROUGH */
	default:
	    if (any("(=~", c)) {
		unreadc(c);
		ungetC(HIST);
		return (0);
	    }
	    np = lhsb;
	    event = 0;
	    while (!cmap(c, _ESC | _META | _QF | _QB) && !any("${}:", c)) {
		if (event != -1 && Isdigit(c))
		    event = event * 10 + c - '0';
		else
		    event = -1;
		if (np < &lhsb[sizeof(lhsb) / sizeof(Char) - 2])
		    *np++ = (Char)c;
		c = getC(0);
	    }
	    unreadc(c);
	    if (np == lhsb) {
		ungetC(HIST);
		return (0);
	    }
	    *np++ = 0;
	    if (event != -1) {
		/*
		 * History had only digits
		 */
		if (back)
		    event = eventno + (alhistp == 0) - (event ? event : 0);
		break;
	    }
	    hp = findev(lhsb, 0);
	    if (hp)
		lastev = hp->Hnum;
	    return (&hp->Hlex);
	case '?':
	    np = lhsb;
	    for (;;) {
		c = getC(0);
		if (c == '\n') {
		    unreadc(c);
		    break;
		}
		if (c == '?')
		    break;
		if (np < &lhsb[sizeof(lhsb) / sizeof(Char) - 2])
		    *np++ = (Char)c;
	    }
	    if (np == lhsb) {
		if (lhsb[0] == 0) {
		    seterror(ERR_NOSEARCH);
		    return (0);
		}
	    }
	    else
		*np++ = 0;
	    hp = findev(lhsb, 1);
	    if (hp)
		lastev = hp->Hnum;
	    return (&hp->Hlex);
	}

    for (hp = Histlist.Hnext; hp; hp = hp->Hnext)
	if (hp->Hnum == event) {
	    hp->Href = eventno;
	    lastev = hp->Hnum;
	    return (&hp->Hlex);
	}
    np = putn(event);
    str = vis_str(np);
    xfree((ptr_t) np);
    seterror(ERR_NOEVENT, str);
    return (0);
}

static struct Hist *
findev(Char *cp, int anyarg)
{
    struct Hist *hp;

    for (hp = Histlist.Hnext; hp; hp = hp->Hnext) {
	Char *dp, *p, *q;
	struct wordent *lp;
	int argno;

	lp = hp->Hlex.next;
	argno = 0;

	/*
	 * The entries added by alias substitution don't have a newline but do
	 * have a negative event number. Savehist() trims off these entries,
	 * but it happens before alias expansion, too early to delete those
	 * from the previous command.
	 */
	if (hp->Hnum < 0)
	    continue;
	if (lp->word[0] == '\n')
	    continue;
	if (!anyarg) {
	    p = cp;
	    q = lp->word;
	    do
		if (!*p)
		    return (hp);
	    while (*p++ == *q++);
	    continue;
	}
	do {
	    for (dp = lp->word; *dp; dp++) {
		p = cp;
		q = dp;
		do
		    if (!*p) {
			quesarg = argno;
			return (hp);
		    }
		while (*p++ == *q++);
	    }
	    lp = lp->next;
	    argno++;
	} while (lp->word[0] != '\n');
    }
    seterror(ERR_NOEVENT, vis_str(cp));
    return (0);
}


static void
setexclp(Char *cp)
{
    if (cp && cp[0] == '\n')
	return;
    exclp = cp;
}

void
unreadc(int c)
{
    peekread = c;
}

int
readc(int wanteof)
{
    static int sincereal;
    int c;

    aret = F_SEEK;
    if ((c = peekread) != '\0') {
	peekread = 0;
	return (c);
    }
top:
    aret = F_SEEK;
    if (alvecp) {
	aret = A_SEEK;
	if ((c = *alvecp++) != '\0')
	    return (c);
	if (alvec && *alvec) {
		alvecp = *alvec++;
		return (' ');
	}
	else {
	    aret = F_SEEK;
	    alvecp = NULL;
	    return('\n');
	}
    }
    if (alvec) {
	if ((alvecp = *alvec) != '\0') {
	    alvec++;
	    goto top;
	}
	/* Infinite source! */
	return ('\n');
    }
    if (evalp) {
	aret = E_SEEK;
	if ((c = *evalp++) != '\0')
	    return (c);
	if (evalvec && *evalvec) {
	    evalp = *evalvec++;
	    return (' ');
	}
	aret = F_SEEK;
	evalp = 0;
    }
    if (evalvec) {
	if (evalvec == (Char **) 1) {
	    doneinp = 1;
	    reset();
	}
	if ((evalp = *evalvec) != '\0') {
	    evalvec++;
	    goto top;
	}
	evalvec = (Char **) 1;
	return ('\n');
    }
    do {
	if (arginp == (Char *) 1 || onelflg == 1) {
	    if (wanteof)
		return (-1);
	    exitstat();
	}
	if (arginp) {
	    if ((c = *arginp++) == 0) {
		arginp = (Char *) 1;
		return ('\n');
	    }
	    return (c);
	}
reread:
	c = bgetc();
	if (c < 0) {
	    struct termios tty;
	    if (wanteof)
		return (-1);
	    /* was isatty but raw with ignoreeof yields problems */
	    if (tcgetattr(SHIN, &tty) == 0 && (tty.c_lflag & ICANON))
	    {
		/* was 'short' for FILEC */
		pid_t     ctpgrp;

		if (++sincereal > 25)
		    goto oops;
		if (tpgrp != -1 &&
		    (ctpgrp = tcgetpgrp(FSHTTY)) != -1 &&
		    tpgrp != ctpgrp) {
		    (void)tcsetpgrp(FSHTTY, tpgrp);
		    (void)kill(-ctpgrp, SIGHUP);
		    (void)fprintf(csherr, "Reset tty pgrp from %ld to %ld\n",
				   (long)ctpgrp, (long)tpgrp);
		    goto reread;
		}
		if (adrof(STRignoreeof)) {
		    if (loginsh)
			(void)fprintf(csherr,"\nUse \"logout\" to logout.\n");
		    else
			(void)fprintf(csherr,"\nUse \"exit\" to leave csh.\n");
		    reset();
		}
		if (chkstop == 0)
		    panystop(1);
	    }
    oops:
	    doneinp = 1;
	    reset();
	}
	sincereal = 0;
	if (c == '\n' && onelflg)
	    onelflg--;
    } while (c == 0);
    return (c);
}

static int
bgetc(void)
{
#ifdef FILEC
    char tbuf[BUFSIZE + 1];
    Char ttyline[BUFSIZE];
    int buf, off;
    ssize_t c, numleft, roomleft;

    numleft = 0; 
#else /* FILEC */
    char tbuf[BUFSIZE + 1];
    int c, buf, off;        
#endif /* !FILEC */

    if (cantell) {
	if (fseekp < fbobp || fseekp > feobp) {
	    fbobp = feobp = fseekp;
	    (void)lseek(SHIN, fseekp, SEEK_SET);
	}
	if (fseekp == feobp) {
	    int i;

	    fbobp = feobp;
	    do
		c = read(SHIN, tbuf, BUFSIZE);
	    while (c < 0 && errno == EINTR);
	    if (c <= 0)
		return (-1);
	    for (i = 0; i < c; i++)
		fbuf[0][i] = (unsigned char) tbuf[i];
	    feobp += c;
	}
	c = fbuf[0][fseekp - fbobp];
	fseekp++;
	return (int)(c);
    }

again:
    buf = (int) fseekp / BUFSIZE;
    if (buf >= fblocks) {
	Char **nfbuf;

	nfbuf = (Char **)xcalloc((size_t) (fblocks + 2), sizeof(char **));
	if (fbuf) {
	    (void)blkcpy(nfbuf, fbuf);
	    xfree((ptr_t) fbuf);
	}
	fbuf = nfbuf;
	fbuf[fblocks] = (Char *)xcalloc(BUFSIZE, sizeof(Char));
	fblocks++;
	if (!intty)
	    goto again;
    }
    if (fseekp >= feobp) {
	buf = (int) feobp / BUFSIZE;
	off = (int) feobp % BUFSIZE;
	roomleft = BUFSIZE - off;

#ifdef FILEC
	for (;;) {
	    if ((editing || filec) && intty) {
#ifdef EDIT
		if (editing) {
			const char *p;
			int d;
			if ((p = el_gets(el, &d)) != NULL) {
				size_t i;
				/* XXX: Truncation */
				numleft = d > BUFSIZE ? BUFSIZE : d;
				for (i = 0; *p && i < BUFSIZE; i++, p++)
					ttyline[i] = *p;
				ttyline[i - (i == BUFSIZE)] = '\0';
			}
		}
#endif
		c = numleft ? numleft : tenex(ttyline, BUFSIZE);
		if (c > roomleft) {
		    /* start with fresh buffer */
		    feobp = fseekp = fblocks * BUFSIZE;
		    numleft = c;
		    goto again;
		}
		if (c > 0)
		    (void)memcpy(fbuf[buf] + off, ttyline,
			(size_t)c * sizeof(**fbuf));
		numleft = 0;
	    }
	    else {
#endif
		c = read(SHIN, tbuf, (size_t)roomleft);
		if (c > 0) {
		    int     i;
		    Char   *ptr = fbuf[buf] + off;

		    for (i = 0; i < c; i++)
			ptr[i] = (unsigned char) tbuf[i];
		}
#ifdef FILEC
	    }
#endif
	    if (c >= 0)
		break;
	    if (errno == EWOULDBLOCK) {
		int     iooff = 0;

		(void)ioctl(SHIN, FIONBIO, (ioctl_t) & iooff);
	    }
	    else if (errno != EINTR)
		break;
#ifdef FILEC
	}
#endif
	if (c <= 0)
	    return (-1);
	feobp += c;
#ifndef FILEC
	goto again;
#else
	if (filec && !intty)
	    goto again;
#endif
    }
    c = fbuf[buf][(int)fseekp % BUFSIZE];
    fseekp++;
    return (int)(c);
}

static void
bfree(void)
{
    int i, sb;

    if (cantell)
	return;
    if (whyles)
	return;
    sb = (int)(fseekp - 1) / BUFSIZE;
    if (sb > 0) {
	for (i = 0; i < sb; i++)
	    xfree((ptr_t) fbuf[i]);
	(void)blkcpy(fbuf, &fbuf[sb]);
	fseekp -= BUFSIZE * sb;
	feobp -= BUFSIZE * sb;
	fblocks -= sb;
    }
}

void
bseek(struct Ain *l)
{
    switch (aret = l->type) {
    case A_SEEK:
	alvec = l->a_seek;
	alvecp = l->c_seek;
	return;
    case E_SEEK:
	evalvec = l->a_seek;
	evalp = l->c_seek;
	return;
    case F_SEEK:
	fseekp = l->f_seek;
	return;
    default:
	(void)fprintf(csherr, "Bad seek type %d\n", aret);
	abort();
    }
}

void
btell(struct Ain *l)
{
    switch (l->type = aret) {
    case A_SEEK:
	l->a_seek = alvec;
	l->c_seek = alvecp;
	return;
    case E_SEEK:
	l->a_seek = evalvec;
	l->c_seek = evalp;
	return;
    case F_SEEK:
	l->f_seek = fseekp;
	l->a_seek = NULL;
	return;
    default:
	(void)fprintf(csherr, "Bad seek type %d\n", aret);
	abort();
    }
}

void
btoeof(void)
{
    (void)lseek(SHIN, (off_t) 0, SEEK_END);
    aret = F_SEEK;
    fseekp = feobp;
    alvec = NULL;
    alvecp = NULL;
    evalvec = NULL;
    evalp = NULL;
    wfree();
    bfree();
}

void
settell(void)
{
    cantell = 0;
    if (arginp || onelflg || intty)
	return;
    if (lseek(SHIN, (off_t) 0, SEEK_CUR) < 0 || errno == ESPIPE)
	return;
    fbuf = (Char **)xcalloc(2, sizeof(Char **));
    fblocks = 1;
    fbuf[0] = (Char *)xcalloc(BUFSIZE, sizeof(Char));
    fseekp = fbobp = feobp = lseek(SHIN, (off_t) 0, SEEK_CUR);
    cantell = 1;
}
