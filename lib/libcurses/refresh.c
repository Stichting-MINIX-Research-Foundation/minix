/*	$NetBSD: refresh.c,v 1.73 2010/02/08 20:45:22 roy Exp $	*/

/*
 * Copyright (c) 1981, 1993, 1994
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
static char sccsid[] = "@(#)refresh.c	8.7 (Berkeley) 8/13/94";
#else
__RCSID("$NetBSD: refresh.c,v 1.73 2010/02/08 20:45:22 roy Exp $");
#endif
#endif				/* not lint */

#include <stdlib.h>
#include <string.h>

#include "curses.h"
#include "curses_private.h"

static void	domvcur(int, int, int, int);
static int	makech(int);
static void	quickch(void);
static void	scrolln(int, int, int, int, int);

static int _cursesi_wnoutrefresh(SCREEN *, WINDOW *,
				 int, int, int, int, int, int);

#ifdef HAVE_WCHAR
int cellcmp( __LDATA *, __LDATA * );
int linecmp( __LDATA *, __LDATA *, size_t );
#endif /* HAVE_WCHAR */

#ifndef _CURSES_USE_MACROS

/*
 * refresh --
 *	Make the current screen look like "stdscr" over the area covered by
 *	stdscr.
 */
int
refresh(void)
{
	return wrefresh(stdscr);
}

#endif

/*
 * wnoutrefresh --
 *	Add the contents of "win" to the virtual window.
 */
int
wnoutrefresh(WINDOW *win)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "wnoutrefresh: win %p\n", win);
#endif

	return _cursesi_wnoutrefresh(_cursesi_screen, win, 0, 0, win->begy,
	    win->begx, win->maxy, win->maxx);
}

/*
 * pnoutrefresh --
 *	Add the contents of "pad" to the virtual window.
 */
int
pnoutrefresh(WINDOW *pad, int pbegy, int pbegx, int sbegy, int sbegx,
	int smaxy, int smaxx)
{
	int pmaxy, pmaxx;

#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "pnoutrefresh: pad %p, flags 0x%08x\n",
	    pad, pad->flags);
	__CTRACE(__CTRACE_REFRESH,
	    "pnoutrefresh: (%d, %d), (%d, %d), (%d, %d)\n",
	    pbegy, pbegx, sbegy, sbegx, smaxy, smaxx);
#endif

	/* SUS says if these are negative, they should be treated as zero */
	if (pbegy < 0)
		pbegy = 0;
	if (pbegx < 0)
		pbegx = 0;
	if (sbegy < 0)
		sbegy = 0;
	if (sbegx < 0)
		sbegx = 0;

	/* Calculate rectangle on pad - used by _cursesi_wnoutrefresh */
	pmaxy = pbegy + smaxy - sbegy + 1;
	pmaxx = pbegx + smaxx - sbegx + 1;

	/* Check rectangle fits in pad */
	if (pmaxy > pad->maxy - pad->begy)
		pmaxy = pad->maxy - pad->begy;
	if (pmaxx > pad->maxx - pad->begx)
		pmaxx = pad->maxx - pad->begx;

	if (smaxy - sbegy < 0 || smaxx - sbegx < 0 )
		return ERR;

	return _cursesi_wnoutrefresh(_cursesi_screen, pad,
	    pad->begy + pbegy, pad->begx + pbegx, pad->begy + sbegy,
	    pad->begx + sbegx, pmaxy, pmaxx);
}

/*
 * _cursesi_wnoutrefresh --
 *	Does the grunt work for wnoutrefresh to the given screen.
 *	Copies the part of the window given by the rectangle
 *	(begy, begx) to (maxy, maxx) at screen position (wbegy, wbegx).
 */
int
_cursesi_wnoutrefresh(SCREEN *screen, WINDOW *win, int begy, int begx,
	int wbegy, int wbegx, int maxy, int maxx)
{

	short	sy, wy, wx, y_off, x_off, mx;
	__LINE	*wlp, *vlp;
	WINDOW	*sub_win, *orig;

#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "_wnoutrefresh: win %p, flags 0x%08x\n",
	    win, win->flags);
	__CTRACE(__CTRACE_REFRESH,
	    "_wnoutrefresh: (%d, %d), (%d, %d), (%d, %d)\n",
	    begy, begx, wbegy, wbegx, maxy, maxx);
#endif

	if (screen->curwin)
		return OK;

	/*
	 * Recurse through any sub-windows, mark as dirty lines on the parent
	 * window that are dirty on the sub-window and clear the dirty flag on
	 * the sub-window.
	 */
	if (win->orig == 0) {
		orig = win;
		for (sub_win = win->nextp; sub_win != win;
		    sub_win = sub_win->nextp) {
#ifdef DEBUG
			__CTRACE(__CTRACE_REFRESH,
			    "wnout_refresh: win %p, sub_win %p\n",
			    orig, sub_win);
#endif
			for (sy = 0; sy < sub_win->maxy; sy++) {
				if (sub_win->alines[sy]->flags == __ISDIRTY) {
					orig->alines[sy + sub_win->begy - orig->begy]->flags
					    |= __ISDIRTY;
					sub_win->alines[sy]->flags
					    &= ~__ISDIRTY;
				}
			}
		}
	}

	/* Check that cursor position on "win" is valid for "__virtscr" */
	if (win->cury + wbegy - begy < screen->__virtscr->maxy &&
	    win->cury + wbegy - begy >= 0 && win->cury < maxy - begy)
		screen->__virtscr->cury = win->cury + wbegy - begy;
	if (win->curx + wbegx - begx < screen->__virtscr->maxx &&
	    win->curx + wbegx - begx >= 0 && win->curx < maxx - begx)
		screen->__virtscr->curx = win->curx + wbegx - begx;

	/* Copy the window flags from "win" to "__virtscr" */
	if (win->flags & __CLEAROK) {
		if (win->flags & __FULLWIN)
			screen->__virtscr->flags |= __CLEAROK;
		win->flags &= ~__CLEAROK;
	}
	screen->__virtscr->flags &= ~__LEAVEOK;
	screen->__virtscr->flags |= win->flags;

	for (wy = begy, y_off = wbegy; wy < maxy &&
	    y_off < screen->__virtscr->maxy; wy++, y_off++) {
		wlp = win->alines[wy];
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH,
		    "_wnoutrefresh: wy %d\tf %d\tl %d\tflags %x\n",
		    wy, *wlp->firstchp, *wlp->lastchp, wlp->flags);
#endif
		if ((wlp->flags & __ISDIRTY) == 0)
			continue;
		vlp = screen->__virtscr->alines[y_off];

		if (*wlp->firstchp < maxx + win->ch_off &&
		    *wlp->lastchp >= win->ch_off) {
			/* Set start column */
			wx = begx;
			x_off = wbegx;
			if (*wlp->firstchp - win->ch_off > 0) {
				wx += *wlp->firstchp - win->ch_off;
				x_off += *wlp->firstchp - win->ch_off;
			}
			/* Set finish column */
			mx = maxx;
			if (mx > *wlp->lastchp - win->ch_off + 1)
				mx = *wlp->lastchp - win->ch_off + 1;
			if (x_off + (mx - wx) > __virtscr->maxx)
				mx -= (x_off + maxx) - __virtscr->maxx;
			/* Copy line from "win" to "__virtscr". */
			while (wx < mx) {
#ifdef DEBUG
				__CTRACE(__CTRACE_REFRESH,
				    "_wnoutrefresh: copy from %d, "
				    "%d to %d, %d\n",
				    wy, wx, y_off, x_off);
#endif
				/* Copy character */
				vlp->line[x_off].ch = wlp->line[wx].ch;
				/* Copy attributes  */
				vlp->line[x_off].attr = wlp->line[wx].attr;
				/* Check for nca conflict with colour */
				if ((vlp->line[x_off].attr & __COLOR) &&
				    (vlp->line[x_off].attr &
				    _cursesi_screen->nca))
					vlp->line[x_off].attr &= ~__COLOR;
#ifdef HAVE_WCHAR
				if (wlp->line[wx].ch
				    == (wchar_t)btowc((int) win->bch)) {
					vlp->line[x_off].ch = win->bch;
					SET_WCOL( vlp->line[x_off], 1 );
					if (_cursesi_copy_nsp(win->bnsp,
							      &vlp->line[x_off])
					    == ERR)
						return ERR;
				}
#endif /* HAVE_WCHAR */
				wx++;
				x_off++;
			}

			/* Set flags on "__virtscr" and unset on "win". */
			if (wlp->flags & __ISPASTEOL)
				vlp->flags |= __ISPASTEOL;
			else
				vlp->flags &= ~__ISPASTEOL;
			if (wlp->flags & __ISDIRTY)
				vlp->flags |= __ISDIRTY;

#ifdef DEBUG
			__CTRACE(__CTRACE_REFRESH,
			    "win: firstch = %d, lastch = %d\n",
			    *wlp->firstchp, *wlp->lastchp);
#endif
			/* Set change pointers on "__virtscr". */
			if (*vlp->firstchp >
			    *wlp->firstchp + wbegx - win->ch_off)
				*vlp->firstchp =
				    *wlp->firstchp + wbegx - win->ch_off;
			if (*vlp->lastchp <
			    *wlp->lastchp + wbegx - win->ch_off)
				*vlp->lastchp =
				    *wlp->lastchp + wbegx - win->ch_off;
#ifdef DEBUG
			__CTRACE(__CTRACE_REFRESH,
			    "__virtscr: firstch = %d, lastch = %d\n",
			    *vlp->firstchp, *vlp->lastchp);
#endif
			/*
			 * Unset change pointers only if a window, as a pad
			 * can be displayed again without any of the contents
			 * changing.
			 */
			if (!(win->flags & __ISPAD)) {
				/* Set change pointers on "win". */
				if (*wlp->firstchp >= win->ch_off)
					*wlp->firstchp = maxx + win->ch_off;
				if (*wlp->lastchp < maxx + win->ch_off)
					*wlp->lastchp = win->ch_off;
				if ((*wlp->lastchp < *wlp->firstchp) ||
				    (*wlp->firstchp >= maxx + win->ch_off) ||
				    (*wlp->lastchp <= win->ch_off)) {
#ifdef DEBUG
					__CTRACE(__CTRACE_REFRESH,
					    "_wnoutrefresh: "
					    "line %d notdirty\n", wy);
#endif
					wlp->flags &= ~__ISDIRTY;
				}
			}
		}
	}
	return OK;
}

/*
 * wrefresh --
 *	Make the current screen look like "win" over the area covered by
 *	win.
 */
int
wrefresh(WINDOW *win)
{
	int retval;

#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "wrefresh: win %p\n", win);
#endif

	_cursesi_screen->curwin = (win == _cursesi_screen->curscr);
	if (!_cursesi_screen->curwin)
		retval = _cursesi_wnoutrefresh(_cursesi_screen, win, 0, 0,
		    win->begy, win->begx, win->maxy, win->maxx);
	else
		retval = OK;
	if (retval == OK) {
		retval = doupdate();
		if (!(win->flags & __LEAVEOK)) {
			win->cury = max(0, curscr->cury - win->begy);
			win->curx = max(0, curscr->curx - win->begx);
		}
	}
	_cursesi_screen->curwin = 0;
	return(retval);
}

 /*
 * prefresh --
 *	Make the current screen look like "pad" over the area coverd by
 *	the specified area of pad.
 */
int
prefresh(WINDOW *pad, int pbegy, int pbegx, int sbegy, int sbegx,
	int smaxy, int smaxx)
{
	int retval;

#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "prefresh: pad %p, flags 0x%08x\n",
	    pad, pad->flags);
#endif
	/* Retain values in case pechochar() is called. */
	pad->pbegy = pbegy;
	pad->pbegx = pbegx;
	pad->sbegy = sbegy;
	pad->sbegx = sbegx;
	pad->smaxy = smaxy;
	pad->smaxx = smaxx;

	/* Use pnoutrefresh() to avoid duplicating code here */
	retval = pnoutrefresh(pad, pbegy, pbegx, sbegy, sbegx, smaxy, smaxx);
	if (retval == OK) {
		retval = doupdate();
		if (!(pad->flags & __LEAVEOK)) {
			pad->cury = max(0, curscr->cury - pad->begy);
			pad->curx = max(0, curscr->curx - pad->begx);
		}
	}
	return(retval);
}

/*
 * doupdate --
 *	Make the current screen look like the virtual window "__virtscr".
 */
int
doupdate(void)
{
	WINDOW	*win;
	__LINE	*wlp;
	short	 wy;
	int	 dnum;
#ifdef HAVE_WCHAR
	__LDATA *lp;
	nschar_t *np;
	int x;
#endif /* HAVE_WCHAR */

	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->curwin)
		win = curscr;
	else
		win = _cursesi_screen->__virtscr;

	/* Initialize loop parameters. */
	_cursesi_screen->ly = curscr->cury;
	_cursesi_screen->lx = curscr->curx;
	wy = 0;

	if (!_cursesi_screen->curwin) {
		for (wy = 0; wy < win->maxy; wy++) {
			wlp = win->alines[wy];
			if (wlp->flags & __ISDIRTY) {
#ifndef HAVE_WCHAR
				wlp->hash = __hash(wlp->line,
				    (size_t)(win->maxx * __LDATASIZE));
#else
				wlp->hash = 0;
				for ( x = 0; x < win->maxx; x++ ) {
					lp = &wlp->line[ x ];
					wlp->hash = __hash_more( &lp->ch,
						sizeof( wchar_t ), wlp->hash );
					wlp->hash = __hash_more( &lp->attr,
						sizeof( attr_t ), wlp->hash );
					np = lp->nsp;
					if (np) {
						while ( np ) {
							wlp->hash
							    = __hash_more(
								&np->ch,
								sizeof(wchar_t),
								wlp->hash );
							np = np->next;
						}
					}
				}
#endif /* HAVE_WCHAR */
			}
		}
	}

	if ((win->flags & __CLEAROK) || (curscr->flags & __CLEAROK) ||
	    _cursesi_screen->curwin) {
		if (curscr->wattr & __COLOR)
			__unsetattr(0);
		tputs(clear_screen, 0, __cputchar);
		_cursesi_screen->ly = 0;
		_cursesi_screen->lx = 0;
		if (!_cursesi_screen->curwin) {
			curscr->flags &= ~__CLEAROK;
			curscr->cury = 0;
			curscr->curx = 0;
			werase(curscr);
		}
		__touchwin(win);
		win->flags &= ~__CLEAROK;
	}
	if (!cursor_address) {
		if (win->curx != 0)
			__cputchar('\n');
		if (!_cursesi_screen->curwin)
			werase(curscr);
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "doupdate: (%p): curwin = %d\n", win,
	    _cursesi_screen->curwin);
	__CTRACE(__CTRACE_REFRESH, "doupdate: \tfirstch\tlastch\n");
#endif

	if (!_cursesi_screen->curwin) {
		/*
		 * Invoke quickch() only if more than a quarter of the lines
		 * in the window are dirty.
		 */
		for (wy = 0, dnum = 0; wy < win->maxy; wy++)
			if (win->alines[wy]->flags & __ISDIRTY)
				dnum++;
		if (!__noqch && dnum > (int) win->maxy / 4)
			quickch();
	}

#ifdef DEBUG
	{
		int	i, j;

		__CTRACE(__CTRACE_REFRESH,
		    "#####################################\n");
		__CTRACE(__CTRACE_REFRESH,
		    "stdscr(%p)-curscr(%p)-__virtscr(%p)\n",
		    stdscr, curscr, _cursesi_screen->__virtscr);
		for (i = 0; i < curscr->maxy; i++) {
			__CTRACE(__CTRACE_REFRESH, "C: %d:", i);
			__CTRACE(__CTRACE_REFRESH, " 0x%x \n",
			    curscr->alines[i]->hash);
			for (j = 0; j < curscr->maxx; j++)
				__CTRACE(__CTRACE_REFRESH, "%c",
				    curscr->alines[i]->line[j].ch);
			__CTRACE(__CTRACE_REFRESH, "\n");
			__CTRACE(__CTRACE_REFRESH, " attr:");
			for (j = 0; j < curscr->maxx; j++)
				__CTRACE(__CTRACE_REFRESH, " %x",
				    curscr->alines[i]->line[j].attr);
			__CTRACE(__CTRACE_REFRESH, "\n");
			__CTRACE(__CTRACE_REFRESH, "W: %d:", i);
			__CTRACE(__CTRACE_REFRESH, " 0x%x \n",
			    win->alines[i]->hash);
			__CTRACE(__CTRACE_REFRESH, " 0x%x ",
			    win->alines[i]->flags);
			for (j = 0; j < win->maxx; j++)
				__CTRACE(__CTRACE_REFRESH, "%c",
				    win->alines[i]->line[j].ch);
			__CTRACE(__CTRACE_REFRESH, "\n");
			__CTRACE(__CTRACE_REFRESH, " attr:");
			for (j = 0; j < win->maxx; j++)
				__CTRACE(__CTRACE_REFRESH, " %x",
				    win->alines[i]->line[j].attr);
			__CTRACE(__CTRACE_REFRESH, "\n");
#ifdef HAVE_WCHAR
			__CTRACE(__CTRACE_REFRESH, " nsp:");
			for (j = 0; j < curscr->maxx; j++)
				__CTRACE(__CTRACE_REFRESH, " %p",
				    win->alines[i]->line[j].nsp);
			__CTRACE(__CTRACE_REFRESH, "\n");
			__CTRACE(__CTRACE_REFRESH, " bnsp:");
			for (j = 0; j < curscr->maxx; j++)
				__CTRACE(__CTRACE_REFRESH, " %p",
				    win->bnsp);
			__CTRACE(__CTRACE_REFRESH, "\n");
#endif /* HAVE_WCHAR */
		}
	}
#endif /* DEBUG */

	for (wy = 0; wy < win->maxy; wy++) {
		wlp = win->alines[wy];
/* XXX: remove this debug */
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH,
		    "doupdate: wy %d\tf: %d\tl:%d\tflags %x\n",
		    wy, *wlp->firstchp, *wlp->lastchp, wlp->flags);
#endif /* DEBUG */
		if (!_cursesi_screen->curwin)
			curscr->alines[wy]->hash = wlp->hash;
		if (wlp->flags & __ISDIRTY) {
#ifdef DEBUG
			__CTRACE(__CTRACE_REFRESH,
			    "doupdate: [ISDIRTY]wy:%d\tf:%d\tl:%d\n", wy,
			    *wlp->firstchp, *wlp->lastchp);
#endif /* DEBUG */
			if (makech(wy) == ERR)
				return (ERR);
			else {
				if (*wlp->firstchp >= 0)
					*wlp->firstchp = win->maxx;
				if (*wlp->lastchp < win->maxx)
					*wlp->lastchp = 0;
				if (*wlp->lastchp < *wlp->firstchp) {
#ifdef DEBUG
					__CTRACE(__CTRACE_REFRESH,
					    "doupdate: line %d notdirty\n", wy);
#endif /* DEBUG */
					wlp->flags &= ~__ISDIRTY;
				}
			}

		}
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH, "\t%d\t%d\n",
		    *wlp->firstchp, *wlp->lastchp);
#endif /* DEBUG */
	}

#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "doupdate: ly=%d, lx=%d\n",
	    _cursesi_screen->ly, _cursesi_screen->lx);
#endif /* DEBUG */

	if (_cursesi_screen->curwin)
		domvcur(_cursesi_screen->ly, _cursesi_screen->lx,
			win->cury, win->curx);
	else {
		if (win->flags & __LEAVEOK) {
			curscr->cury = _cursesi_screen->ly;
			curscr->curx = _cursesi_screen->lx;
		} else {
			domvcur(_cursesi_screen->ly, _cursesi_screen->lx,
				win->cury, win->curx);
			curscr->cury = win->cury;
			curscr->curx = win->curx;
		}
	}

	/* Don't leave the screen with attributes set. */
	__unsetattr(0);
#ifdef DEBUG
#ifdef HAVE_WCHAR
	{
		int	i, j;

		__CTRACE(__CTRACE_REFRESH,
		    "***********after*****************\n");
		__CTRACE(__CTRACE_REFRESH,
		    "stdscr(%p)-curscr(%p)-__virtscr(%p)\n",
		    stdscr, curscr, _cursesi_screen->__virtscr);
		for (i = 0; i < curscr->maxy; i++) {
			for (j = 0; j < curscr->maxx; j++)
				__CTRACE(__CTRACE_REFRESH,
				    "[%d,%d](%x,%x,%p)-(%x,%x,%p)\n",
				    i, j,
				    curscr->alines[i]->line[j].ch,
				    curscr->alines[i]->line[j].attr,
				    curscr->alines[i]->line[j].nsp,
				    _cursesi_screen->__virtscr->alines[i]->line[j].ch,
				    _cursesi_screen->__virtscr->alines[i]->line[j].attr,
				    _cursesi_screen->__virtscr->alines[i]->line[j].nsp);
		}
	}
#endif /* HAVE_WCHAR */
#endif /* DEBUG */
	return fflush(_cursesi_screen->outfd) == EOF ? ERR : OK;
}

/*
 * makech --
 *	Make a change on the screen.
 */
static int
makech(int wy)
{
	WINDOW	*win;
	static __LDATA blank;
	__LDATA *nsp, *csp, *cp, *cep;
	size_t	clsp, nlsp;	/* Last space in lines. */
	int	lch, wx;
	const char	*ce;
	attr_t	lspc;		/* Last space colour */
	attr_t	off, on;

#ifdef __GNUC__
	nlsp = lspc = 0;	/* XXX gcc -Wuninitialized */
#endif
	if (_cursesi_screen->curwin)
		win = curscr;
	else
		win = __virtscr;
#ifdef HAVE_WCHAR
	blank.ch = ( wchar_t )btowc(( int ) win->bch );
	blank.attr = 0;
	if (_cursesi_copy_nsp(win->bnsp, &blank) == ERR)
		return ERR;
	SET_WCOL( blank, 1 );
#endif /* HAVE_WCHAR */
#ifdef DEBUG
#if HAVE_WCHAR
	{
		int x;
		__LDATA *lp, *vlp;

		__CTRACE(__CTRACE_REFRESH,
		    "[makech-before]wy=%d,curscr(%p)-__virtscr(%p)\n",
		    wy, curscr, __virtscr);
		for (x = 0; x < curscr->maxx; x++) {
			lp = &curscr->alines[wy]->line[x];
			vlp = &__virtscr->alines[wy]->line[x];
			__CTRACE(__CTRACE_REFRESH,
			    "[%d,%d](%x,%x,%x,%x,%p)-"
			    "(%x,%x,%x,%x,%p)\n",
			    wy, x, lp->ch, lp->attr,
			    win->bch, win->battr, lp->nsp,
			    vlp->ch, vlp->attr,
			    win->bch, win->battr, vlp->nsp);
		}
	}
#endif /* HAVE_WCHAR */
#endif /* DEBUG */
	/* Is the cursor still on the end of the last line? */
	if (wy > 0 && curscr->alines[wy - 1]->flags & __ISPASTEOL) {
		domvcur(_cursesi_screen->ly, _cursesi_screen->lx,
			_cursesi_screen->ly + 1, 0);
		_cursesi_screen->ly++;
		_cursesi_screen->lx = 0;
	}
	wx = *win->alines[wy]->firstchp;
	if (wx < 0)
		wx = 0;
	else
		if (wx >= win->maxx)
			return (OK);
	lch = *win->alines[wy]->lastchp;
	if (lch < 0)
		return (OK);
	else
		if (lch >= (int) win->maxx)
			lch = win->maxx - 1;

	if (_cursesi_screen->curwin) {
		csp = &blank;
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH, "makech: csp is blank\n");
#endif /* DEBUG */
	} else {
		csp = &curscr->alines[wy]->line[wx];
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH,
		    "makech: csp is on curscr:(%d,%d)\n", wy, wx);
#endif /* DEBUG */
	}

	nsp = &win->alines[wy]->line[wx];
#ifdef DEBUG
	if ( _cursesi_screen->curwin )
		__CTRACE(__CTRACE_REFRESH,
		    "makech: nsp is at curscr:(%d,%d)\n", wy, wx);
	else
		__CTRACE(__CTRACE_REFRESH,
		    "makech: nsp is at __virtscr:(%d,%d)\n", wy, wx);
#endif /* DEBUG */
	if (clr_eol && !_cursesi_screen->curwin) {
		cp = &win->alines[wy]->line[win->maxx - 1];
		lspc = cp->attr & __COLOR;
#ifndef HAVE_WCHAR
		while (cp->ch == ' ' && cp->attr == lspc) /* XXX */
			if (cp-- <= win->alines[wy]->line)
				break;
#else
		while (cp->ch == ( wchar_t )btowc(( int )' ' )
				&& ( cp->attr & WA_ATTRIBUTES ) == lspc)
			if (cp-- <= win->alines[wy]->line)
				break;
#endif /* HAVE_WCHAR */
		if (win->alines[wy]->line > cp)
			nlsp = 0;
		else
			nlsp = cp - win->alines[wy]->line;
	}
	if (!_cursesi_screen->curwin)
		ce = clr_eol;
	else
		ce = NULL;

	while (wx <= lch) {
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH, "makech: wx=%d,lch=%d\n", wx, lch);
#endif /* DEBUG */
#ifndef HAVE_WCHAR
		if (memcmp(nsp, csp, sizeof(__LDATA)) == 0) {
			if (wx <= lch) {
				while (wx <= lch &&
				    memcmp(nsp, csp, sizeof(__LDATA)) == 0) {
					nsp++;
					if (!_cursesi_screen->curwin)
						++csp;
					++wx;
				}
				continue;
			}
			break;
		}
#else
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH, "makech: nsp=(%x,%x,%x,%x,%p)\n",
			nsp->ch, nsp->attr, win->bch, win->battr, nsp->nsp);
		__CTRACE(__CTRACE_REFRESH, "makech: csp=(%x,%x,%x,%x,%p)\n",
			csp->ch, csp->attr, win->bch, win->battr, csp->nsp);
#endif /* DEBUG */
		if (((nsp->attr & __WCWIDTH) != __WCWIDTH) &&
		    cellcmp(nsp, csp)) {
			if (wx <= lch) {
				while (wx <= lch && cellcmp( csp, nsp )) {
					nsp++;
					if (!_cursesi_screen->curwin)
						++csp;
					++wx;
				}
				continue;
			}
			break;
		}
#endif /* HAVE_WCHAR */
		domvcur(_cursesi_screen->ly, _cursesi_screen->lx, wy, wx);

#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH, "makech: 1: wx = %d, ly= %d, "
		    "lx = %d, newy = %d, newx = %d\n",
		    wx, _cursesi_screen->ly, _cursesi_screen->lx, wy, wx);
#endif
		_cursesi_screen->ly = wy;
		_cursesi_screen->lx = wx;
#ifndef HAVE_WCHAR
		while (wx <= lch && memcmp(nsp, csp, sizeof(__LDATA)) != 0) {
			if (ce != NULL &&
			    wx >= nlsp && nsp->ch == ' ' && nsp->attr == lspc) {
#else
		while (!cellcmp(nsp, csp) && wx <= lch) {
			if (ce != NULL && wx >= nlsp
			   && nsp->ch == (wchar_t)btowc((int)' ') /* XXX */
			   && (nsp->attr & WA_ATTRIBUTES) == lspc) {

#endif
				/* Check for clear to end-of-line. */
				cep = &curscr->alines[wy]->line[win->maxx - 1];
#ifndef HAVE_WCHAR
				while (cep->ch == ' ' && cep->attr == lspc) /* XXX */
#else
				while (cep->ch == (wchar_t)btowc((int)' ')
				       && (cep->attr & WA_ATTRIBUTES) == lspc)
#endif /* HAVE_WCHAR */
					if (cep-- <= csp)
						break;
				clsp = cep - curscr->alines[wy]->line -
				    win->begx * __LDATASIZE;
#ifdef DEBUG
				__CTRACE(__CTRACE_REFRESH,
				    "makech: clsp = %zu, nlsp = %zu\n",
				    clsp, nlsp);
#endif
				if (((clsp - nlsp >= strlen(clr_eol) &&
				    clsp < win->maxx * __LDATASIZE) ||
				    wy == win->maxy - 1) &&
				    (!(lspc & __COLOR) ||
				    ((lspc & __COLOR) && back_color_erase))) {
					__unsetattr(0);
					if (__using_color &&
					    ((lspc & __COLOR) !=
					    (curscr->wattr & __COLOR)))
						__set_color(curscr, lspc &
						    __COLOR);
					tputs(clr_eol, 0, __cputchar);
					_cursesi_screen->lx = wx + win->begx;
					while (wx++ <= clsp) {
						csp->attr = lspc;
#ifndef HAVE_WCHAR
						csp->ch = ' '; /* XXX */
#else
						csp->ch = (wchar_t)btowc((int)' ');
						SET_WCOL( *csp, 1 );
#endif /* HAVE_WCHAR */
						csp++;
					}
					return (OK);
				}
				ce = NULL;
			}

#ifdef DEBUG
				__CTRACE(__CTRACE_REFRESH,
				    "makech: have attr %08x, need attr %08x\n",
				    curscr->wattr & WA_ATTRIBUTES,
				    nsp->attr & WA_ATTRIBUTES);
#endif

			off = (~nsp->attr & curscr->wattr)
#ifndef HAVE_WCHAR
				 & __ATTRIBUTES
#else
				 & WA_ATTRIBUTES
#endif
				;

			/*
			 * Unset attributes as appropriate.  Unset first
			 * so that the relevant attributes can be reset
			 * (because 'me' unsets 'mb', 'md', 'mh', 'mk',
			 * 'mp' and 'mr').  Check to see if we also turn off
			 * standout, attributes and colour.
			 */
			if (off & __TERMATTR && exit_attribute_mode != NULL) {
				tputs(exit_attribute_mode, 0, __cputchar);
				curscr->wattr &= __mask_me;
				off &= __mask_me;
			}

			/*
			 * Exit underscore mode if appropriate.
			 * Check to see if we also turn off standout,
			 * attributes and colour.
			 */
			if (off & __UNDERSCORE && exit_underline_mode != NULL) {
				tputs(exit_underline_mode, 0, __cputchar);
				curscr->wattr &= __mask_ue;
				off &= __mask_ue;
			}

			/*
			 * Exit standout mode as appropriate.
			 * Check to see if we also turn off underscore,
			 * attributes and colour.
			 * XXX
			 * Should use uc if so/se not available.
			 */
			if (off & __STANDOUT && exit_standout_mode != NULL) {
				tputs(exit_standout_mode, 0, __cputchar);
				curscr->wattr &= __mask_se;
				off &= __mask_se;
			}

			if (off & __ALTCHARSET && exit_alt_charset_mode != NULL) {
				tputs(exit_alt_charset_mode, 0, __cputchar);
				curscr->wattr &= ~__ALTCHARSET;
			}

			/* Set/change colour as appropriate. */
			if (__using_color)
				__set_color(curscr, nsp->attr & __COLOR);

			on = (nsp->attr & ~curscr->wattr)
#ifndef HAVE_WCHAR
				 & __ATTRIBUTES
#else
				 & WA_ATTRIBUTES
#endif
				 ;

			/*
			 * Enter standout mode if appropriate.
			 */
			if (on & __STANDOUT &&
			    enter_standout_mode != NULL &&
			    exit_standout_mode != NULL)
			{
				tputs(enter_standout_mode, 0, __cputchar);
				curscr->wattr |= __STANDOUT;
			}

			/*
			 * Enter underscore mode if appropriate.
			 * XXX
			 * Should use uc if us/ue not available.
			 */
			if (on & __UNDERSCORE &&
			    enter_underline_mode != NULL &&
			    exit_underline_mode != NULL)
			{
				tputs(enter_underline_mode, 0, __cputchar);
				curscr->wattr |= __UNDERSCORE;
			}

			/*
			 * Set other attributes as appropriate.
			 */
			if (exit_attribute_mode != NULL) {
				if (on & __BLINK &&
				    enter_blink_mode != NULL)
				{
					tputs(enter_blink_mode, 0, __cputchar);
					curscr->wattr |= __BLINK;
				}
				if (on & __BOLD &&
				    enter_bold_mode != NULL)
				{
					tputs(enter_bold_mode, 0, __cputchar);
					curscr->wattr |= __BOLD;
				}
				if (on & __DIM &&
				    enter_dim_mode != NULL)
				{
					tputs(enter_dim_mode, 0, __cputchar);
					curscr->wattr |= __DIM;
				}
				if (on & __BLANK &&
				    enter_secure_mode != NULL)
				{
					tputs(enter_secure_mode, 0, __cputchar);
					curscr->wattr |= __BLANK;
				}
				if (on & __PROTECT &&
				    enter_protected_mode != NULL)
				{
					tputs(enter_protected_mode, 0, __cputchar);
					curscr->wattr |= __PROTECT;
				}
				if (on & __REVERSE &&
				    enter_reverse_mode != NULL)
				{
					tputs(enter_reverse_mode, 0, __cputchar);
					curscr->wattr |= __REVERSE;
				}
#ifdef HAVE_WCHAR
				if (on & WA_TOP &&
				    enter_top_hl_mode != NULL)
				{
					tputs(enter_top_hl_mode, 0, __cputchar);
					curscr->wattr |= WA_TOP;
				}
				if (on & WA_LOW &&
				    enter_low_hl_mode != NULL)
				{
					tputs(enter_low_hl_mode, 0, __cputchar);
					curscr->wattr |= WA_LOW;
				}
				if (on & WA_LEFT &&
				    enter_left_hl_mode != NULL)
				{
					tputs(enter_left_hl_mode, 0, __cputchar);
					curscr->wattr |= WA_LEFT;
				}
				if (on & WA_RIGHT &&
				    enter_right_hl_mode != NULL)
				{
					tputs(enter_right_hl_mode, 0, __cputchar);
					curscr->wattr |= WA_RIGHT;
				}
				if (on & WA_HORIZONTAL &&
				    enter_horizontal_hl_mode != NULL)
				{
					tputs(enter_horizontal_hl_mode, 0, __cputchar);
					curscr->wattr |= WA_HORIZONTAL;
				}
				if (on & WA_VERTICAL &&
				    enter_vertical_hl_mode != NULL)
				{
					tputs(enter_vertical_hl_mode, 0, __cputchar);
					curscr->wattr |= WA_VERTICAL;
				}
#endif /* HAVE_WCHAR */
			}

			/* Enter/exit altcharset mode as appropriate. */
			if (on & __ALTCHARSET && enter_alt_charset_mode != NULL &&
			    exit_alt_charset_mode != NULL) {
				tputs(enter_alt_charset_mode, 0, __cputchar);
				curscr->wattr |= __ALTCHARSET;
			}

			wx++;
			if (wx >= win->maxx &&
			    wy == win->maxy - 1 && !_cursesi_screen->curwin) {
				if (win->flags & __SCROLLOK) {
					if (win->flags & __ENDLINE)
						__unsetattr(1);
					if (!(win->flags & __SCROLLWIN)) {
						if (!_cursesi_screen->curwin) {
							csp->attr = nsp->attr;
							csp->ch = nsp->ch;
#ifdef HAVE_WCHAR
							if (_cursesi_copy_nsp(nsp->nsp, csp) == ERR)
								return ERR;
#endif /* HAVE_WCHAR */
						}
#ifndef HAVE_WCHAR
						__cputchar((int) nsp->ch);
#else
						if ( WCOL( *nsp ) > 0 ) {
							__cputwchar((int)nsp->ch);
#ifdef DEBUG
							__CTRACE(__CTRACE_REFRESH,
							    "makech: (%d,%d)putwchar(0x%x)\n",
								wy, wx - 1,
								nsp->ch );
#endif /* DEBUG */
							/*
							 * Output non-spacing
							 * characters for the
							 * cell.
							 */
							__cursesi_putnsp(nsp->nsp,
									 wy, wx);

						}
#endif /* HAVE_WCHAR */
					}
					if (wx < curscr->maxx) {
						domvcur(_cursesi_screen->ly, wx,
						    (int) (win->maxy - 1),
						    (int) (win->maxx - 1));
					}
					_cursesi_screen->ly = win->maxy - 1;
					_cursesi_screen->lx = win->maxx - 1;
					return (OK);
				}
			}
			if (wx < win->maxx || wy < win->maxy - 1 ||
			    !(win->flags & __SCROLLWIN)) {
				if (!_cursesi_screen->curwin) {
					csp->attr = nsp->attr;
					csp->ch = nsp->ch;
#ifdef HAVE_WCHAR
					if (_cursesi_copy_nsp(nsp->nsp,
							      csp) == ERR)
						return ERR;
#endif /* HAVE_WCHAR */
					csp++;
				}
#ifndef HAVE_WCHAR
				__cputchar((int) nsp->ch);
#ifdef DEBUG
				__CTRACE(__CTRACE_REFRESH,
				    "makech: putchar(%c)\n", nsp->ch & 0177);
#endif
#else
				if (WCOL(*nsp) > 0) {
					__cputwchar((int) nsp->ch);
#ifdef DEBUG
					__CTRACE(__CTRACE_REFRESH,
					    "makech:(%d,%d) putwchar(%x)\n",
					    wy, wx - 1, nsp->ch);
					__cursesi_putnsp(nsp->nsp, wy, wx);
#endif /* DEBUG */
				}
#endif /* HAVE_WCHAR */
			}
			if (underline_char && ((nsp->attr & __STANDOUT) ||
			    (nsp->attr & __UNDERSCORE))) {
				__cputchar('\b');
				tputs(underline_char, 0, __cputchar);
			}
			nsp++;
#ifdef DEBUG
			__CTRACE(__CTRACE_REFRESH,
			    "makech: 2: wx = %d, lx = %d\n",
			    wx, _cursesi_screen->lx);
#endif
		}
		if (_cursesi_screen->lx == wx)	/* If no change. */
			break;
		_cursesi_screen->lx = wx;
		if (_cursesi_screen->lx >= COLS && auto_right_margin)
			_cursesi_screen->lx = COLS - 1;
		else
			if (wx >= win->maxx) {
				domvcur(_cursesi_screen->ly,
					_cursesi_screen->lx,
					_cursesi_screen->ly,
					(int) (win->maxx - 1));
				_cursesi_screen->lx = win->maxx - 1;
			}
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH, "makech: 3: wx = %d, lx = %d\n",
		    wx, _cursesi_screen->lx);
#endif
	}
#ifdef DEBUG
#if HAVE_WCHAR
	{
		int x;
		__LDATA *lp, *vlp;

		__CTRACE(__CTRACE_REFRESH,
		    "makech-after: curscr(%p)-__virtscr(%p)\n",
		    curscr, __virtscr );
		for (x = 0; x < curscr->maxx; x++) {
			lp = &curscr->alines[wy]->line[x];
			vlp = &__virtscr->alines[wy]->line[x];
			__CTRACE(__CTRACE_REFRESH,
			    "[%d,%d](%x,%x,%x,%x,%p)-"
			    "(%x,%x,%x,%x,%p)\n",
			    wy, x, lp->ch, lp->attr,
			    win->bch, win->battr, lp->nsp,
			    vlp->ch, vlp->attr,
			    win->bch, win->battr, vlp->nsp);
		}
	}
#endif /* HAVE_WCHAR */
#endif /* DEBUG */

	return (OK);
}

/*
 * domvcur --
 *	Do a mvcur, leaving attributes if necessary.
 */
static void
domvcur(oy, ox, ny, nx)
	int	oy, ox, ny, nx;
{
#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "domvcur: (%x,%d)=>(%d,%d)\n",
	    oy, ox, ny, nx );
#endif /* DEBUG */
	__unsetattr(1);
	if ( oy == ny && ox == nx )
		return;
	__mvcur(oy, ox, ny, nx, 1);
}

/*
 * Quickch() attempts to detect a pattern in the change of the window
 * in order to optimize the change, e.g., scroll n lines as opposed to
 * repainting the screen line by line.
 */

static __LDATA buf[128];
static  u_int last_hash;
static  size_t last_hash_len;
#define BLANKSIZE (sizeof(buf) / sizeof(buf[0]))

static void
quickch(void)
{
#define THRESH		(int) __virtscr->maxy / 4

	__LINE *clp, *tmp1, *tmp2;
	int	bsize, curs, curw, starts, startw, i, j;
	int	n, target, cur_period, bot, top, sc_region;
	u_int	blank_hash;
	attr_t	bcolor;

#ifdef __GNUC__
	curs = curw = starts = startw = 0;	/* XXX gcc -Wuninitialized */
#endif
	/*
	 * Find how many lines from the top of the screen are unchanged.
	 */
	for (top = 0; top < __virtscr->maxy; top++)
#ifndef HAVE_WCHAR
		if (__virtscr->alines[top]->flags & __ISDIRTY &&
		    (__virtscr->alines[top]->hash != curscr->alines[top]->hash ||
		    memcmp(__virtscr->alines[top]->line,
		    curscr->alines[top]->line,
		    (size_t) __virtscr->maxx * __LDATASIZE)
		    != 0))
			break;
#else
		if (__virtscr->alines[top]->flags & __ISDIRTY &&
		    (__virtscr->alines[top]->hash != curscr->alines[top]->hash ||
		    !linecmp(__virtscr->alines[top]->line,
		    curscr->alines[top]->line,
	(size_t) __virtscr->maxx )))
			break;
#endif /* HAVE_WCHAR */
		else
			__virtscr->alines[top]->flags &= ~__ISDIRTY;
	/*
	 * Find how many lines from bottom of screen are unchanged.
	 */
	for (bot = __virtscr->maxy - 1; bot >= 0; bot--)
#ifndef HAVE_WCHAR
		if (__virtscr->alines[bot]->flags & __ISDIRTY &&
		    (__virtscr->alines[bot]->hash != curscr->alines[bot]->hash ||
		    memcmp(__virtscr->alines[bot]->line,
		    curscr->alines[bot]->line,
		    (size_t) __virtscr->maxx * __LDATASIZE)
		    != 0))
			break;
#else
		if (__virtscr->alines[bot]->flags & __ISDIRTY &&
		    (__virtscr->alines[bot]->hash != curscr->alines[bot]->hash ||
		    !linecmp(__virtscr->alines[bot]->line,
		    curscr->alines[bot]->line,
		    (size_t) __virtscr->maxx )))
			break;
#endif /* HAVE_WCHAR */
		else
			__virtscr->alines[bot]->flags &= ~__ISDIRTY;

	/*
	 * Work round an xterm bug where inserting lines causes all the
	 * inserted lines to be covered with the background colour we
	 * set on the first line (even if we unset it for subsequent
	 * lines).
	 */
	bcolor = __virtscr->alines[min(top,
	    __virtscr->maxy - 1)]->line[0].attr & __COLOR;
	for (i = top + 1, j = 0; i < bot; i++) {
		if ((__virtscr->alines[i]->line[0].attr & __COLOR) != bcolor) {
			bcolor = __virtscr->alines[i]->line[__virtscr->maxx].
			    attr & __COLOR;
			j = i - top;
		} else
			break;
	}
	top += j;

#ifdef NO_JERKINESS
	/*
	 * If we have a bottom unchanged region return.  Scrolling the
	 * bottom region up and then back down causes a screen jitter.
	 * This will increase the number of characters sent to the screen
	 * but it looks better.
	 */
	if (bot < __virtscr->maxy - 1)
		return;
#endif				/* NO_JERKINESS */

	/*
	 * Search for the largest block of text not changed.
	 * Invariants of the loop:
	 * - Startw is the index of the beginning of the examined block in
	 *   __virtscr.
	 * - Starts is the index of the beginning of the examined block in
	 *   curscr.
	 * - Curw is the index of one past the end of the exmined block in
	 *   __virtscr.
	 * - Curs is the index of one past the end of the exmined block in
	 *   curscr.
	 * - bsize is the current size of the examined block.
	*/

	for (bsize = bot - top; bsize >= THRESH; bsize--) {
		for (startw = top; startw <= bot - bsize; startw++)
			for (starts = top; starts <= bot - bsize;
				starts++) {
				for (curw = startw, curs = starts;
				    curs < starts + bsize; curw++, curs++)
					if (__virtscr->alines[curw]->hash !=
					    curscr->alines[curs]->hash)
						break;
				if (curs != starts + bsize)
					continue;
				for (curw = startw, curs = starts;
					curs < starts + bsize; curw++, curs++)
#ifndef HAVE_WCHAR
					if (memcmp(__virtscr->alines[curw]->line,
					    curscr->alines[curs]->line,
					    (size_t) __virtscr->maxx *
					    __LDATASIZE) != 0)
						break;
#else
					if (!linecmp(__virtscr->alines[curw]->line,
					    curscr->alines[curs]->line,
					    (size_t) __virtscr->maxx))
						break;
#endif /* HAVE_WCHAR */
				if (curs == starts + bsize)
					goto done;
			}
	}
done:

	/* Did not find anything */
	if (bsize < THRESH)
		return;

#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "quickch:bsize=%d, starts=%d, startw=%d, "
	    "curw=%d, curs=%d, top=%d, bot=%d\n",
	    bsize, starts, startw, curw, curs, top, bot);
#endif

	/*
	 * Make sure that there is no overlap between the bottom and top
	 * regions and the middle scrolled block.
	 */
	if (bot < curs)
		bot = curs - 1;
	if (top > starts)
		top = starts;

	n = startw - starts;

#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "#####################################\n");
	for (i = 0; i < curscr->maxy; i++) {
		__CTRACE(__CTRACE_REFRESH, "C: %d:", i);
		__CTRACE(__CTRACE_REFRESH, " 0x%x \n", curscr->alines[i]->hash);
		for (j = 0; j < curscr->maxx; j++)
			__CTRACE(__CTRACE_REFRESH, "%c",
			    curscr->alines[i]->line[j].ch);
		__CTRACE(__CTRACE_REFRESH, "\n");
		__CTRACE(__CTRACE_REFRESH, " attr:");
		for (j = 0; j < curscr->maxx; j++)
			__CTRACE(__CTRACE_REFRESH, " %x",
			    curscr->alines[i]->line[j].attr);
		__CTRACE(__CTRACE_REFRESH, "\n");
		__CTRACE(__CTRACE_REFRESH, "W: %d:", i);
		__CTRACE(__CTRACE_REFRESH, " 0x%x \n",
		    __virtscr->alines[i]->hash);
		__CTRACE(__CTRACE_REFRESH, " 0x%x ",
		    __virtscr->alines[i]->flags);
		for (j = 0; j < __virtscr->maxx; j++)
			__CTRACE(__CTRACE_REFRESH, "%c",
			    __virtscr->alines[i]->line[j].ch);
		__CTRACE(__CTRACE_REFRESH, "\n");
		__CTRACE(__CTRACE_REFRESH, " attr:");
		for (j = 0; j < __virtscr->maxx; j++)
			__CTRACE(__CTRACE_REFRESH, " %x",
			    __virtscr->alines[i]->line[j].attr);
		__CTRACE(__CTRACE_REFRESH, "\n");
	}
#endif

#ifndef HAVE_WCHAR
	if (buf[0].ch != ' ') {
		for (i = 0; i < BLANKSIZE; i++) {
			buf[i].ch = ' ';
			buf[i].attr = 0;
		}
	}
#else
	if (buf[0].ch != ( wchar_t )btowc(( int ) curscr->bch )) {
		for (i = 0; i < BLANKSIZE; i++) {
			buf[i].ch = ( wchar_t )btowc(( int ) curscr->bch );
			if (_cursesi_copy_nsp(curscr->bnsp, &buf[i]) == ERR)
				return;
			buf[i].attr = 0;
			SET_WCOL( buf[ i ], 1 );
		}
	}
#endif /* HAVE_WCHAR */

	if (__virtscr->maxx != last_hash_len) {
		blank_hash = 0;
		for (i = __virtscr->maxx; i > BLANKSIZE; i -= BLANKSIZE) {
			blank_hash = __hash_more(buf, sizeof(buf), blank_hash);
		}
		blank_hash = __hash_more((char *)(void *)buf,
		    i * sizeof(buf[0]), blank_hash);
		/* cache result in static data - screen width doesn't change often */
		last_hash_len = __virtscr->maxx;
		last_hash = blank_hash;
	} else
		blank_hash = last_hash;

	/*
	 * Perform the rotation to maintain the consistency of curscr.
	 * This is hairy since we are doing an *in place* rotation.
	 * Invariants of the loop:
	 * - I is the index of the current line.
	 * - Target is the index of the target of line i.
	 * - Tmp1 points to current line (i).
	 * - Tmp2 and points to target line (target);
	 * - Cur_period is the index of the end of the current period.
	 *   (see below).
	 *
	 * There are 2 major issues here that make this rotation non-trivial:
	 * 1.  Scrolling in a scrolling region bounded by the top
	 *     and bottom regions determined (whose size is sc_region).
	 * 2.  As a result of the use of the mod function, there may be a
	 *     period introduced, i.e., 2 maps to 4, 4 to 6, n-2 to 0, and
	 *     0 to 2, which then causes all odd lines not to be rotated.
	 *     To remedy this, an index of the end ( = beginning) of the
	 *     current 'period' is kept, cur_period, and when it is reached,
	 *     the next period is started from cur_period + 1 which is
	 *     guaranteed not to have been reached since that would mean that
	 *     all records would have been reached. (think about it...).
	 *
	 * Lines in the rotation can have 3 attributes which are marked on the
	 * line so that curscr is consistent with the visual screen.
	 * 1.  Not dirty -- lines inside the scrolled block, top region or
	 *                  bottom region.
	 * 2.  Blank lines -- lines in the differential of the scrolling
	 *		      region adjacent to top and bot regions
	 *                    depending on scrolling direction.
	 * 3.  Dirty line -- all other lines are marked dirty.
	 */
	sc_region = bot - top + 1;
	i = top;
	tmp1 = curscr->alines[top];
	cur_period = top;
	for (j = top; j <= bot; j++) {
		target = (i - top + n + sc_region) % sc_region + top;
		tmp2 = curscr->alines[target];
		curscr->alines[target] = tmp1;
		/* Mark block as clean and blank out scrolled lines. */
		clp = curscr->alines[target];
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH,
		    "quickch: n=%d startw=%d curw=%d i = %d target=%d ",
		    n, startw, curw, i, target);
#endif
		if ((target >= startw && target < curw) || target < top
		    || target > bot) {
#ifdef DEBUG
			__CTRACE(__CTRACE_REFRESH, " notdirty\n");
#endif
			__virtscr->alines[target]->flags &= ~__ISDIRTY;
		} else
			if ((n > 0 && target >= top && target < top + n) ||
			    (n < 0 && target <= bot && target > bot + n)) {
#ifndef HAVE_WCHAR
				if (clp->hash != blank_hash ||
				    memcmp(clp->line, clp->line + 1,
				    (__virtscr->maxx - 1)
				    * __LDATASIZE) ||
				    memcmp(clp->line, buf, __LDATASIZE)) {
#else
				if (clp->hash != blank_hash
				    || linecmp(clp->line, clp->line + 1,
				    (unsigned int) (__virtscr->maxx - 1))
				    || cellcmp(clp->line, buf)) {
#endif /* HAVE_WCHAR */
					for (i = __virtscr->maxx;
					    i > BLANKSIZE;
					    i -= BLANKSIZE) {
						(void) memcpy(clp->line + i -
						    BLANKSIZE, buf, sizeof(buf));
					}
					(void) memcpy(clp->line , buf, i *
					    sizeof(buf[0]));
#ifdef DEBUG
					__CTRACE(__CTRACE_REFRESH,
					    " blanked out: dirty\n");
#endif
					clp->hash = blank_hash;
					__touchline(__virtscr, target, 0, (int) __virtscr->maxx - 1);
				} else {
#ifdef DEBUG
					__CTRACE(__CTRACE_REFRESH,
					    " -- blank line already: dirty\n");
#endif
					__touchline(__virtscr, target, 0, (int) __virtscr->maxx - 1);
				}
			} else {
#ifdef DEBUG
				__CTRACE(__CTRACE_REFRESH, " -- dirty\n");
#endif
				__touchline(__virtscr, target, 0, (int) __virtscr->maxx - 1);
			}
		if (target == cur_period) {
			i = target + 1;
			tmp1 = curscr->alines[i];
			cur_period = i;
		} else {
			tmp1 = tmp2;
			i = target;
		}
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
	for (i = 0; i < curscr->maxy; i++) {
		__CTRACE(__CTRACE_REFRESH, "C: %d:", i);
		for (j = 0; j < curscr->maxx; j++)
			__CTRACE(__CTRACE_REFRESH, "%c",
			    curscr->alines[i]->line[j].ch);
		__CTRACE(__CTRACE_REFRESH, "\n");
		__CTRACE(__CTRACE_REFRESH, "W: %d:", i);
		for (j = 0; j < __virtscr->maxx; j++)
			__CTRACE(__CTRACE_REFRESH, "%c",
			    __virtscr->alines[i]->line[j].ch);
		__CTRACE(__CTRACE_REFRESH, "\n");
	}
#endif
	if (n != 0)
		scrolln(starts, startw, curs, bot, top);
}

/*
 * scrolln --
 *	Scroll n lines, where n is starts - startw.
 */
static void /* ARGSUSED */
scrolln(starts, startw, curs, bot, top)
	int	starts, startw, curs, bot, top;
{
	int	i, oy, ox, n;

	oy = curscr->cury;
	ox = curscr->curx;
	n = starts - startw;

	/*
	 * XXX
	 * The initial tests that set __noqch don't let us reach here unless
	 * we have either cs + ho + SF/sf/SR/sr, or AL + DL.  SF/sf and SR/sr
	 * scrolling can only shift the entire scrolling region, not just a
	 * part of it, which means that the quickch() routine is going to be
	 * sadly disappointed in us if we don't have cs as well.
	 *
	 * If cs, ho and SF/sf are set, can use the scrolling region.  Because
	 * the cursor position after cs is undefined, we need ho which gives us
	 * the ability to move to somewhere without knowledge of the current
	 * location of the cursor.  Still call __mvcur() anyway, to update its
	 * idea of where the cursor is.
	 *
	 * When the scrolling region has been set, the cursor has to be at the
	 * last line of the region to make the scroll happen.
	 *
	 * Doing SF/SR or AL/DL appears faster on the screen than either sf/sr
	 * or AL/DL, and, some terminals have AL/DL, sf/sr, and cs, but not
	 * SF/SR.  So, if we're scrolling almost all of the screen, try and use
	 * AL/DL, otherwise use the scrolling region.  The "almost all" is a
	 * shameless hack for vi.
	 */
	if (n > 0) {
		if (change_scroll_region != NULL && cursor_home != NULL &&
		    (parm_index != NULL ||
		    ((parm_insert_line == NULL || parm_delete_line == NULL ||
		    top > 3 || bot + 3 < __virtscr->maxy) &&
		    scroll_forward != NULL)))
		{
			tputs(vtparm(change_scroll_region, top, bot),
			    0, __cputchar);
			__mvcur(oy, ox, 0, 0, 1);
			tputs(cursor_home, 0, __cputchar);
			__mvcur(0, 0, bot, 0, 1);
			if (parm_index != NULL)
				tputs(vtparm(parm_index, n),
				    0, __cputchar);
			else
				for (i = 0; i < n; i++)
					tputs(scroll_forward, 0, __cputchar);
			tputs(vtparm(change_scroll_region,
			    0, (int) __virtscr->maxy - 1), 0, __cputchar);
			__mvcur(bot, 0, 0, 0, 1);
			tputs(cursor_home, 0, __cputchar);
			__mvcur(0, 0, oy, ox, 1);
			return;
		}

		/* Scroll up the block. */
		if (parm_index != NULL && top == 0) {
			__mvcur(oy, ox, bot, 0, 1);
			tputs(vtparm(parm_index, n), 0, __cputchar);
		} else
			if (parm_delete_line != NULL) {
				__mvcur(oy, ox, top, 0, 1);
				tputs(vtparm(parm_delete_line, n),
				    0, __cputchar);
			} else
				if (delete_line != NULL) {
					__mvcur(oy, ox, top, 0, 1);
					for (i = 0; i < n; i++)
						tputs(delete_line,
						    0, __cputchar);
				} else
					if (scroll_forward != NULL && top == 0) {
						__mvcur(oy, ox, bot, 0, 1);
						for (i = 0; i < n; i++)
							tputs(scroll_forward, 0,
							    __cputchar);
					} else
						abort();

		/* Push down the bottom region. */
		__mvcur(top, 0, bot - n + 1, 0, 1);
		if (parm_insert_line != NULL)
			tputs(vtparm(parm_insert_line, n), 0, __cputchar);
		else
			if (insert_line != NULL)
				for (i = 0; i < n; i++)
					tputs(insert_line, 0, __cputchar);
			else
				abort();
		__mvcur(bot - n + 1, 0, oy, ox, 1);
	} else {
		/*
		 * !!!
		 * n < 0
		 *
		 * If cs, ho and SR/sr are set, can use the scrolling region.
		 * See the above comments for details.
		 */
		if (change_scroll_region != NULL && cursor_home != NULL &&
		    (parm_rindex != NULL ||
		    ((parm_insert_line == NULL || parm_delete_line == NULL ||
		    top > 3 ||
		    bot + 3 < __virtscr->maxy) && scroll_reverse != NULL)))
		{
			tputs(vtparm(change_scroll_region, top, bot),
			    0, __cputchar);
			__mvcur(oy, ox, 0, 0, 1);
			tputs(cursor_home, 0, __cputchar);
			__mvcur(0, 0, top, 0, 1);

			if (parm_rindex != NULL)
				tputs(vtparm(parm_rindex, -n),
				    0, __cputchar);
			else
				for (i = n; i < 0; i++)
					tputs(scroll_reverse, 0, __cputchar);
			tputs(vtparm(change_scroll_region,
			    0, (int) __virtscr->maxy - 1), 0, __cputchar);
			__mvcur(top, 0, 0, 0, 1);
			tputs(cursor_home, 0, __cputchar);
			__mvcur(0, 0, oy, ox, 1);
			return;
		}

		/* Preserve the bottom lines. */
		__mvcur(oy, ox, bot + n + 1, 0, 1);
		if (parm_rindex != NULL && bot == __virtscr->maxy)
			tputs(vtparm(parm_rindex, -n), 0, __cputchar);
		else
			if (parm_delete_line != NULL)
				tputs(vtparm(parm_delete_line, -n),
				    0, __cputchar);
			else
				if (delete_line != NULL)
					for (i = n; i < 0; i++)
						tputs(delete_line,
						    0, __cputchar);
				else
					if (scroll_reverse != NULL &&
					    bot == __virtscr->maxy)
						for (i = n; i < 0; i++)
							tputs(scroll_reverse, 0,
							    __cputchar);
					else
						abort();

		/* Scroll the block down. */
		__mvcur(bot + n + 1, 0, top, 0, 1);
		if (parm_insert_line != NULL)
			tputs(vtparm(parm_insert_line, -n), 0, __cputchar);
		else
			if (insert_line != NULL)
				for (i = n; i < 0; i++)
					tputs(insert_line, 0, __cputchar);
			else
				abort();
		__mvcur(top, 0, oy, ox, 1);
	}
}

/*
 * __unsetattr --
 *	Unset attributes on curscr.  Leave standout, attribute and colour
 *	modes if necessary (!ms).  Always leave altcharset (xterm at least
 *	ignores a cursor move if we don't).
 */
void /* ARGSUSED */
__unsetattr(int checkms)
{
	int	isms;

	if (checkms)
		if (!move_standout_mode) {
			isms = 1;
		} else {
			isms = 0;
		}
	else
		isms = 1;
#ifdef DEBUG
	__CTRACE(__CTRACE_REFRESH,
	    "__unsetattr: checkms = %d, ms = %s, wattr = %08x\n",
	    checkms, move_standout_mode ? "TRUE" : "FALSE", curscr->wattr);
#endif

	/*
	 * Don't leave the screen in standout mode (check against ms).  Check
	 * to see if we also turn off underscore, attributes and colour.
	 */
	if (curscr->wattr & __STANDOUT && isms) {
		tputs(exit_standout_mode, 0, __cputchar);
		curscr->wattr &= __mask_se;
	}
	/*
	 * Don't leave the screen in underscore mode (check against ms).
	 * Check to see if we also turn off attributes.  Assume that we
	 * also turn off colour.
	 */
	if (curscr->wattr & __UNDERSCORE && isms) {
		tputs(exit_underline_mode, 0, __cputchar);
		curscr->wattr &= __mask_ue;
	}
	/*
	 * Don't leave the screen with attributes set (check against ms).
	 * Assume that also turn off colour.
	 */
	if (curscr->wattr & __TERMATTR && isms) {
		tputs(exit_attribute_mode, 0, __cputchar);
		curscr->wattr &= __mask_me;
	}
	/* Don't leave the screen with altcharset set (don't check ms). */
	if (curscr->wattr & __ALTCHARSET) {
		tputs(exit_alt_charset_mode, 0, __cputchar);
		curscr->wattr &= ~__ALTCHARSET;
	}
	/* Don't leave the screen with colour set (check against ms). */
	if (__using_color && isms)
		__unset_color(curscr);
}

#ifdef HAVE_WCHAR
/* compare two cells on screen, must have the same forground/background,
 * and the same sequence of non-spacing characters */
int
cellcmp( __LDATA *x, __LDATA *y )
{
	nschar_t *xnp = x->nsp, *ynp = y->nsp;
	int ret = ( x->ch == y->ch ) & ( x->attr == y->attr );

	if ( !ret )
		return 0;
	if ( !xnp && !ynp )
		return 1;
	if (( xnp && !ynp ) || ( !xnp && ynp ))
		return 0;

	while ( xnp && ynp ) {
		if ( xnp->ch != ynp->ch )
			return 0;
		xnp = xnp->next;
		ynp = ynp->next;
	}
	return ( !xnp && !ynp );
}

/* compare two line segments */
int
linecmp( __LDATA *xl, __LDATA *yl, size_t len )
{
	int i = 0;
	__LDATA *xp = xl, *yp = yl;

	for ( i = 0; i < len; i++, xp++, yp++ ) {
		if ( !cellcmp( xp, yp ))
			return 0;
	}
	return 1;
}

/*
 * Output the non-spacing characters associated with the given character
 * cell to the screen.
 */

void
__cursesi_putnsp(nschar_t *nsp, const int wy, const int wx)
{
	nschar_t *p;

	/* this shuts up gcc warnings about wx and wy not being used */
	if (wx > wy) {
	}

	p = nsp;
	while (p != NULL) {
		__cputwchar((int) p->ch);
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH,
		       "_cursesi_putnsp: (%d,%d) non-spacing putwchar(0x%x)\n",
			 wy, wx - 1, p->ch);
#endif
		p = p->next;
	}
}

#endif /* HAVE_WCHAR */
