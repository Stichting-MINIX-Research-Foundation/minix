/*   $NetBSD: ins_wstr.c,v 1.6 2010/12/16 17:42:28 wiz Exp $ */

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
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the NetBSD Foundation nor the names of its
 *	contributors may be used to endorse or promote products derived
 *	from this software without specific prior written permission.
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
__RCSID("$NetBSD: ins_wstr.c,v 1.6 2010/12/16 17:42:28 wiz Exp $");
#endif						  /* not lint */

#include <string.h>
#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"

/*
 * ins_wstr --
 *	insert a multi-character wide-character string into the current window
 */
int
ins_wstr(const wchar_t *wstr)
{
	return wins_wstr(stdscr, wstr);
}

/*
 * ins_nwstr --
 *	insert a multi-character wide-character string into the current window
 *	with at most n characters
 */
int
ins_nwstr(const wchar_t *wstr, int n)
{
	return wins_nwstr(stdscr, wstr, n);
}

/*
 * mvins_wstr --
 *	  Do an insert-string on the line at (y, x).
 */
int
mvins_wstr(int y, int x, const wchar_t *wstr)
{
	return mvwins_wstr(stdscr, y, x, wstr);
}

/*
 * mvins_nwstr --
 *	  Do an insert-n-string on the line at (y, x).
 */
int
mvins_nwstr(int y, int x, const wchar_t *wstr, int n)
{
	return mvwins_nwstr(stdscr, y, x, wstr, n);
}

/*
 * mvwins_wstr --
 *	  Do an insert-string on the line at (y, x) in the given window.
 */
int
mvwins_wstr(WINDOW *win, int y, int x, const wchar_t *wstr)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wins_wstr(stdscr, wstr);
}

/*
 * mvwins_nwstr --
 *	  Do an insert-n-string on the line at (y, x) in the given window.
 */
int
mvwins_nwstr(WINDOW *win, int y, int x, const wchar_t *wstr, int n)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wins_nwstr(stdscr, wstr, n);
}


/*
 * wins_wstr --
 *	Do an insert-string on the line, leaving (cury, curx) unchanged.
 */
int
wins_wstr(WINDOW *win, const wchar_t *wstr)
{
	return wins_nwstr(win, wstr, -1);
}

/*
 * wins_nwstr --
 *	Do an insert-n-string on the line, leaving (cury, curx) unchanged.
 */
int
wins_nwstr(WINDOW *win, const wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	__LDATA	 *start, *temp1, *temp2;
	__LINE	  *lnp;
	const wchar_t *scp;
	int width, len, sx, x, y, cw, pcw, newx;
	nschar_t *np;
	wchar_t ws[] = L"		";

	/* check for leading non-spacing character */
	if (!wstr)
		return OK;
	cw = wcwidth(*wstr);
	if (cw < 0)
		cw = 1;
	if (!cw)
		return ERR;

	scp = wstr + 1;
	width = cw;
	len = 1;
	n--;
	while (*scp) {
		int w;
		if (!n)
			break;
		w = wcwidth(*scp);
		if (w < 0)
			w = 1;
		n--, len++, width += w;
		scp++;
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wins_nwstr: width=%d,len=%d\n", width, len);
#endif /* DEBUG */

	if (cw > win->maxx - win->curx + 1)
		return ERR;
	start = &win->alines[win->cury]->line[win->curx];
	sx = win->curx;
	lnp = win->alines[win->cury];
	pcw = WCOL(*start);
	if (pcw < 0) {
		sx += pcw;
		start += pcw;
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wins_nwstr: start@(%d)\n", sx);
#endif /* DEBUG */
	pcw = WCOL(*start);
	lnp->flags |= __ISDIRTY;
	newx = sx + win->ch_off;
	if (newx < *lnp->firstchp)
		*lnp->firstchp = newx;
#ifdef DEBUG
	{
		__CTRACE(__CTRACE_INPUT, "========before=======\n");
		for (x = 0; x < win->maxx; x++)
			__CTRACE(__CTRACE_INPUT,
			    "wins_nwstr: (%d,%d)=(%x,%x,%p)\n",
			    (int) win->cury, x,
			    win->alines[win->cury]->line[x].ch,
			    win->alines[win->cury]->line[x].attr,
			    win->alines[win->cury]->line[x].nsp);
	}
#endif /* DEBUG */

	/* shift all complete characters */
	if (sx + width + pcw <= win->maxx) {
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "wins_nwstr: shift all characters\n");
#endif /* DEBUG */
		temp1 = &win->alines[win->cury]->line[win->maxx - 1];
		temp2 = temp1 - width;
		pcw = WCOL(*(temp2 + 1));
		if (pcw < 0) {
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "wins_nwstr: clear from %d to EOL(%d)\n",
			    win->maxx + pcw, win->maxx - 1);
#endif /* DEBUG */
			temp2 += pcw;
			while (temp1 > temp2 + width) {
				temp1->ch = (wchar_t)btowc((int) win->bch);
				if (_cursesi_copy_nsp(win->bnsp, temp1) == ERR)
					return ERR;
				temp1->attr = win->battr;
				SET_WCOL(*temp1, 1);
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "wins_nwstr: empty cell(%p)\n", temp1);
#endif /* DEBUG */
				temp1--;
			}
		}
		while (temp2 >= start) {
			(void)memcpy(temp1, temp2, sizeof(__LDATA));
			temp1--, temp2--;
		}
#ifdef DEBUG
		{
			__CTRACE(__CTRACE_INPUT, "=====after shift====\n");
			for (x = 0; x < win->maxx; x++)
				__CTRACE(__CTRACE_INPUT,
				    "wins_nwstr: (%d,%d)=(%x,%x,%p)\n",
				    (int) win->cury, x,
				    win->alines[win->cury]->line[x].ch,
				    win->alines[win->cury]->line[x].attr,
				    win->alines[win->cury]->line[x].nsp);
		}
#endif /* DEBUG */
	}

	/* update string columns */
	x = win->curx;
	y = win->cury;
	for (scp = wstr, temp1 = start; len; len--, scp++) {
		switch (*scp) {
			case L'\b':
				if (--x < 0)
					x = 0;
				win->curx = x;
				continue;;
			case L'\r':
				win->curx = 0;
				continue;
			case L'\n':
				wclrtoeol(win);
				if (y == win->scr_b) {
					if (!(win->flags & __SCROLLOK))
						return ERR;
					scroll(win);
				}
				continue;
			case L'\t':
				if (wins_nwstr(win, ws,
						min(win->maxx - x, 8-(x % 8)))
							== ERR)
					return ERR;
				continue;
		}
		cw = wcwidth(*scp);
		if (cw < 0)
			cw = 1;
		if (cw) {
			/* 1st column */
			temp1->ch = (wchar_t)*scp;
			temp1->attr = win->wattr;
			SET_WCOL(*temp1, cw);
			temp1->nsp = NULL;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "wins_nwstr: add spacing char(%x)\n", temp1->ch);
#endif /* DEBUG */
			temp2 = temp1++;
			if (cw > 1) {
				x = -1;
				while (temp1 < temp2 + cw) {
					/* the rest columns */
					temp1->ch = (wchar_t)*scp;
					temp1->attr = win->wattr;
					temp1->nsp = NULL;
					SET_WCOL(*temp1, x);
					temp1++, x--;
				}
				temp1--;
			}
		} else {
			/* non-spacing character */
			np = (nschar_t *)malloc(sizeof(nschar_t));
			if (!np)
				return ERR;
			np->ch = *scp;
			np->next = temp1->nsp;
			temp1->nsp = np;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "wins_nstr: add non-spacing char(%x)\n", np->ch);
#endif /* DEBUG */
		}
	}
#ifdef DEBUG
	{
		__CTRACE(__CTRACE_INPUT, "========after=======\n");
		for (x = 0; x < win->maxx; x++)
			__CTRACE(__CTRACE_INPUT,
			    "wins_nwstr: (%d,%d)=(%x,%x,%p)\n",
			    (int) win->cury, x,
			    win->alines[win->cury]->line[x].ch,
			    win->alines[win->cury]->line[x].attr,
			    win->alines[win->cury]->line[x].nsp);
	}
#endif /* DEBUG */
	newx = win->maxx - 1 + win->ch_off;
	if (newx > *lnp->lastchp)
		*lnp->lastchp = newx;
	__touchline(win, (int) win->cury, sx, (int) win->maxx - 1);
	return OK;
#endif /* HAVE_WCHAR */
}
