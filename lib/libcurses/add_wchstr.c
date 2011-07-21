/*   $NetBSD: add_wchstr.c,v 1.4 2010/02/23 19:48:26 drochner Exp $ */

/*
 * Copyright (c) 2005 The NetBSD Foundation Inc.
 * All rights reserved.
 *
 * This code is derived from code donated to the NetBSD Foundation
 * by Ruibiao Qiu <ruibiao@arl.wustl.edu,ruibiao@gmail.com>.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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
__RCSID("$NetBSD: add_wchstr.c,v 1.4 2010/02/23 19:48:26 drochner Exp $");
#endif				/* not lint */

#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"

/*
 * add_wchstr --
 *  Add a wide string to stdscr starting at (_cury, _curx).
 */
int
add_wchstr(const cchar_t *wchstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wadd_wchnstr(stdscr, wchstr, -1);
#endif
}


/*
 * wadd_wchstr --
 *      Add a string to the given window starting at (_cury, _curx).
 */
int
wadd_wchstr(WINDOW *win, const cchar_t *wchstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wadd_wchnstr(win, wchstr, -1);
#endif
}


/*
 * add_wchnstr --
 *      Add a string (at most n characters) to stdscr starting
 *	at (_cury, _curx).  If n is negative, add the entire string.
 */
int
add_wchnstr(const cchar_t *wchstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wadd_wchnstr(stdscr, wchstr, n);
#endif
}


/*
 * mvadd_wchstr --
 *      Add a string to stdscr starting at (y, x)
 */
int
mvadd_wchstr(int y, int x, const cchar_t *wchstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwadd_wchnstr(stdscr, y, x, wchstr, -1);
#endif
}


/*
 * mvwadd_wchstr --
 *      Add a string to the given window starting at (y, x)
 */
int
mvwadd_wchstr(WINDOW *win, int y, int x, const cchar_t *wchstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwadd_wchnstr(win, y, x, wchstr, -1);
#endif
}


/*
 * mvadd_wchnstr --
 *      Add a string of at most n characters to stdscr
 *      starting at (y, x).
 */
int
mvadd_wchnstr(int y, int x, const cchar_t *wchstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwadd_wchnstr(stdscr, y, x, wchstr, n);
#endif
}


/*
 * mvwadd_wchnstr --
 *      Add a string of at most n characters to the given window
 *      starting at (y, x).
 */
int
mvwadd_wchnstr(WINDOW *win, int y, int x, const cchar_t *wchstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wadd_wchnstr(win, wchstr, n);
#endif
}


/*
 * wadd_wchnstr --
 *	Add a string (at most n wide characters) to the given window
 *	starting at (_cury, _curx).  If n is -1, add the entire string.
 */
int
wadd_wchnstr(WINDOW *win, const cchar_t *wchstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	const cchar_t *chp;
	wchar_t wc;
	int cw, x, y, sx, ex, newx, i, cnt;
	__LDATA *lp, *tp;
	nschar_t *np, *tnp;
	__LINE *lnp;

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT,
	    "wadd_wchnstr: win = %p, wchstr = %p, n = %d\n", win, wchstr, n);
#endif

	if (!wchstr)
		return OK;

	/* compute length of the cchar string */
	if (n < -1)
		return ERR;
	if (n >= 0)
		for (chp = wchstr, cnt = 0; n && chp->vals[0];
			n--, chp++, ++cnt);
	else
		for (chp = wchstr, cnt = 0; chp->vals[0]; chp++, ++cnt);
#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wadd_wchnstr: len=%d\n", cnt);
#endif /* DEBUG */
	chp = wchstr;
	x = win->curx;
	y = win->cury;
	lp = &win->alines[y]->line[x];
	lnp = win->alines[y];

	cw = WCOL(*lp);
	if (cw >= 0) {
		sx = x;
	} else {
		if (wcwidth(chp->vals[0])) {
			/* clear the partial character before cursor */
			for (tp = lp + cw; tp < lp; tp++) {
				tp->ch = (wchar_t) btowc((int) win->bch);
				if (_cursesi_copy_nsp(win->bnsp, tp) == ERR)
					return ERR;
				tp->attr = win->battr;
				SET_WCOL(*tp, 1);
				np = tp->nsp;
			}
		} else {
			/* move to the start of current char */
			lp += cw;
			x += cw;
		}
		sx = x + cw;
	}
	lnp->flags |= __ISDIRTY;
	newx = sx + win->ch_off;
	if (newx < *lnp->firstchp)
		*lnp->firstchp = newx;

	/* add characters in the string */
	ex = x;
	while (cnt) {
		x = ex;
		wc = chp->vals[0];
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "wadd_wchnstr: adding %x", wc);
#endif /* DEBUG */
		cw = wcwidth(wc);
		if (cw < 0)
			cw = 1;
		if (cw) {
			/* spacing character */
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    " as a spacing char(width=%d)\n", cw);
#endif /* DEBUG */
			if (cw > win->maxx - ex) {
				/* clear to EOL */
				while (ex < win->maxx) {
					lp->ch = (wchar_t)
						btowc((int) win->bch);
					if (_cursesi_copy_nsp(win->bnsp, lp)
					    == ERR)
						return ERR;
					lp->attr = win->battr;
					SET_WCOL(*lp, 1);
					lp++, ex++;
				}
				ex = win->maxx - 1;
				break;
			}
			/* this could combine with the insertion of
			 * non-spacing char */
			np = lp->nsp;
			if (np) {
				while (np) {
					tnp = np->next;
					free(np);
					np = tnp;
				}
				lp->nsp = NULL;
			}
			lp->ch = chp->vals[0];
			lp->attr = chp->attributes & WA_ATTRIBUTES;
			SET_WCOL(*lp, cw);
			if (chp->elements > 1) {
				for (i = 1; i < chp->elements; i++) {
					np = (nschar_t *)
						malloc(sizeof(nschar_t));
					if (!np)
						return ERR;
					np->ch = chp->vals[i];
					np->next = lp->nsp;
					lp->nsp = np;
				}
			}
			lp++, ex++;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
				"wadd_wchnstr: ex = %d, x = %d, cw = %d\n",
				 ex, x, cw);
#endif /* DEBUG */
			while (ex - x <= cw - 1) {
				np = lp->nsp;
				if (np) {
					while (np) {
						tnp = np->next;
						free(np);
						np = tnp;
					}
					lp->nsp = NULL;
				}
				lp->ch = chp->vals[0];
				lp->attr = chp->attributes & WA_ATTRIBUTES;
				SET_WCOL(*lp, x - ex);
				lp++, ex++;
			}
		} else {
			/* non-spacing character */
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
				"wadd_wchnstr: as non-spacing char");
#endif /* DEBUG */
			for (i = 0; i < chp->elements; i++) {
				np = (nschar_t *)malloc(sizeof(nschar_t));
				if (!np)
					return ERR;
				np->ch = chp->vals[i];
				np->next = lp->nsp;
				lp->nsp = np;
			}
		}
		cnt--, chp++;
	}
#ifdef DEBUG
	for (i = sx; i < ex; i++) {
		__CTRACE(__CTRACE_INPUT, "wadd_wchnstr: (%d,%d)=(%x,%x,%p)\n",
		    win->cury, i, win->alines[win->cury]->line[i].ch,
		    win->alines[win->cury]->line[i].attr,
		    win->alines[win->cury]->line[i].nsp);
	}
#endif /* DEBUG */
	lnp->flags |= __ISDIRTY;
	newx = ex + win->ch_off;
	if (newx > *lnp->lastchp)
		*lnp->lastchp = newx;
	__touchline(win, y, sx, ex);

	return OK;
#endif /* HAVE_WCHAR */
}
