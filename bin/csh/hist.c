/* $NetBSD: hist.c,v 1.20 2013/07/16 17:47:43 christos Exp $ */

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
static char sccsid[] = "@(#)hist.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: hist.c,v 1.20 2013/07/16 17:47:43 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>

#include <stdarg.h>
#include <stdlib.h>

#include "csh.h"
#include "extern.h"

static void hfree(struct Hist *);
static void dohist1(struct Hist *, int *, int, int);
static void phist(struct Hist *, int);

void
savehist(struct wordent *sp)
{
    struct Hist *hp, *np;
    Char *cp;
    int histlen;

    histlen = 0;

    /* throw away null lines */
    if (sp->next->word[0] == '\n')
	return;
    cp = value(STRhistory);
    if (*cp) {
	Char *p = cp;

	while (*p) {
	    if (!Isdigit(*p)) {
		histlen = 0;
		break;
	    }
	    histlen = histlen * 10 + *p++ - '0';
	}
    }
    for (hp = &Histlist; (np = hp->Hnext) != NULL;)
	if (eventno - np->Href >= histlen || histlen == 0)
	    hp->Hnext = np->Hnext, hfree(np);
	else
	    hp = np;
    (void) enthist(++eventno, sp, 1);
}

#ifdef EDIT
void
loadhist(struct Hist *hp) {
    char *h = NULL;

    if (hi == NULL || hp == NULL)
	return;
    loadhist(hp->Hnext);
    if (sprlex(&h, &hp->Hlex) != -1) {
	HistEvent ev;
	history(hi, &ev, H_ENTER, h);
    }
}
#endif

struct Hist *
enthist(int event, struct wordent *lp, int docopy)
{
    struct Hist *np;

#ifdef EDIT
    if (hi) {
	char *h = NULL;
	if (sprlex(&h, lp) != -1) {
	    HistEvent ev;
	    history(hi, &ev, H_ENTER, h);
	}
    }
#endif
    np = xmalloc(sizeof(*np));
    np->Hnum = np->Href = event;
    if (docopy) {
	copylex(&np->Hlex, lp);
    }
    else {
	np->Hlex.next = lp->next;
	lp->next->prev = &np->Hlex;
	np->Hlex.prev = lp->prev;
	lp->prev->next = &np->Hlex;
    }
    np->Hnext = Histlist.Hnext;
    Histlist.Hnext = np;
    return (np);
}

static void
hfree(struct Hist *hp)
{
    freelex(&hp->Hlex);
    xfree((ptr_t) hp);
}

void
/*ARGSUSED*/
dohist(Char **v, struct command *t)
{
    sigset_t nsigset;
    int hflg, n, rflg;

    hflg = 0;
    rflg = 0;

    if (getn(value(STRhistory)) == 0)
	return;
    if (setintr) {
	sigemptyset(&nsigset);
	(void)sigaddset(&nsigset, SIGINT);
	(void)sigprocmask(SIG_UNBLOCK, &nsigset, NULL);
    }
    while (*++v && **v == '-') {
	Char *vp = *v;

	while (*++vp)
	    switch (*vp) {
	    case 'h':
		hflg++;
		break;
	    case 'r':
		rflg++;
		break;
	    case '-':		/* ignore multiple '-'s */
		break;
	    default:
		stderror(ERR_HISTUS);
		/* NOTREACHED */
	    }
    }
    if (*v)
	n = getn(*v);
    else {
	n = getn(value(STRhistory));
    }
    dohist1(Histlist.Hnext, &n, rflg, hflg);
}

static void
dohist1(struct Hist *hp, int *np, int rflg, int hflg)
{
    int print;

    print = (*np) > 0;

    for (; hp != 0; hp = hp->Hnext) {
	(*np)--;
	hp->Href++;
	if (rflg == 0) {
	    dohist1(hp->Hnext, np, rflg, hflg);
	    if (print)
		phist(hp, hflg);
	    return;
	}
	if (*np >= 0)
	    phist(hp, hflg);
    }
}

static void
phist(struct Hist *hp, int hflg)
{
    if (hflg == 0)
	(void)fprintf(cshout, "%6d\t", hp->Hnum);
    prlex(cshout, &hp->Hlex);
}
