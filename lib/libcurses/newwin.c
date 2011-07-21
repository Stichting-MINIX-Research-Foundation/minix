/*	$NetBSD: newwin.c,v 1.47 2009/07/22 16:57:15 roy Exp $	*/

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
static char sccsid[] = "@(#)newwin.c	8.3 (Berkeley) 7/27/94";
#else
__RCSID("$NetBSD: newwin.c,v 1.47 2009/07/22 16:57:15 roy Exp $");
#endif
#endif				/* not lint */

#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"


static WINDOW *__makenew(SCREEN *screen, int nlines, int ncols, int by,
			 int bx, int sub, int ispad);
static WINDOW *__subwin(WINDOW *orig, int nlines, int ncols, int by, int bx,
			 int ispad);

/*
 * derwin --
 *      Create a new window in the same manner as subwin but (by, bx)
 *      are relative to the origin of window orig instead of absolute.
 */
WINDOW *
derwin(WINDOW *orig, int nlines, int ncols, int by, int bx)
{

	return __subwin(orig, nlines, ncols, orig->begy + by, orig->begx + bx,
	    FALSE);
}

/*
 * subpad --
 *      Create a new pad in the same manner as subwin but (by, bx)
 *      are relative to the origin of window orig instead of absolute.
 */
WINDOW *
subpad(WINDOW *orig, int nlines, int ncols, int by, int bx)
{

	return __subwin(orig, nlines, ncols, orig->begy + by, orig->begx + bx,
	    TRUE);
}

/*
 * dupwin --
 *      Create a copy of the given window.
 */
WINDOW *
dupwin(WINDOW *win)
{
	WINDOW *new_one;

	if ((new_one = __newwin(_cursesi_screen, win->maxy, win->maxx,
				win->begy, win->begx, FALSE)) == NULL)
		return NULL;

	overwrite(win, new_one);
	return new_one;
}

/*
 * newwin --
 *	Allocate space for and set up defaults for a new window.
 */
WINDOW *
newwin(int nlines, int ncols, int by, int bx)
{
	return __newwin(_cursesi_screen, nlines, ncols, by, bx, FALSE);
}

/*
 * newpad --
 *	Allocate space for and set up defaults for a new pad.
 */
WINDOW *
newpad(int nlines, int ncols)
{
	if (nlines < 1 || ncols < 1)
		return NULL;
	return __newwin(_cursesi_screen, nlines, ncols, 0, 0, TRUE);
}

WINDOW *
__newwin(SCREEN *screen, int nlines, int ncols, int by, int bx, int ispad)
{
	WINDOW *win;
	__LINE *lp;
	int     i, j;
	int	maxy, maxx;
	__LDATA *sp;

	if (by < 0 || bx < 0)
		return (NULL);

	maxy = nlines > 0 ? nlines : LINES - by + nlines;
	maxx = ncols > 0 ? ncols : COLS - bx + ncols;

	if ((win = __makenew(screen, maxy, maxx, by, bx, 0, ispad)) == NULL)
		return (NULL);

	win->bch = ' ';
	if (__using_color)
		win->battr = __default_color;
	else
		win->battr = 0;
	win->nextp = win;
	win->ch_off = 0;
	win->orig = NULL;
	win->reqy = nlines;
	win->reqx = ncols;

#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "newwin: win->ch_off = %d\n", win->ch_off);
#endif

	for (i = 0; i < maxy; i++) {
		lp = win->alines[i];
		if (ispad)
			lp->flags = __ISDIRTY;
		else
			lp->flags = 0;
		for (sp = lp->line, j = 0; j < maxx; j++, sp++) {
			sp->attr = 0;
#ifndef HAVE_WCHAR
			sp->ch = win->bch;
#else
			sp->ch = ( wchar_t )btowc(( int ) win->bch );
			sp->nsp = NULL;
			SET_WCOL( *sp, 1 );
#endif /* HAVE_WCHAR */
		}
		lp->hash = __hash((char *)(void *)lp->line,
		    (size_t) (ncols * __LDATASIZE));
	}
	return (win);
}

WINDOW *
subwin(WINDOW *orig, int nlines, int ncols, int by, int bx)
{

	return __subwin(orig, nlines, ncols, by, bx, FALSE);
}

static WINDOW *
__subwin(WINDOW *orig, int nlines, int ncols, int by, int bx, int ispad)
{
	int     i;
	__LINE *lp;
	WINDOW *win;
	int	maxy, maxx;

#ifdef	DEBUG
	__CTRACE(__CTRACE_WINDOW, "subwin: (%p, %d, %d, %d, %d, %d)\n",
	    orig, nlines, ncols, by, bx, ispad);
#endif
	if (orig == NULL || orig->orig != NULL)
		return NULL;

	/* Make sure window fits inside the original one. */
	maxy = nlines > 0 ? nlines : orig->maxy + orig->begy - by + nlines;
	maxx = ncols > 0 ? ncols : orig->maxx + orig->begx - bx + ncols;
	if (by < orig->begy || bx < orig->begx
	    || by + maxy > orig->maxy + orig->begy
	    || bx + maxx > orig->maxx + orig->begx)
		return (NULL);
	if ((win = __makenew(_cursesi_screen, maxy, maxx,
			     by, bx, 1, ispad)) == NULL)
		return (NULL);
	win->bch = orig->bch;
	win->battr = orig->battr;
	win->reqy = nlines;
	win->reqx = ncols;
	win->nextp = orig->nextp;
	orig->nextp = win;
	win->orig = orig;

	/* Initialize flags here so that refresh can also use __set_subwin. */
	for (lp = win->lspace, i = 0; i < win->maxy; i++, lp++)
		lp->flags = 0;
	__set_subwin(orig, win);
	return (win);
}
/*
 * This code is shared with mvwin().
 */
void
__set_subwin(WINDOW *orig, WINDOW *win)
{
	int     i;
	__LINE *lp, *olp;
#ifdef HAVE_WCHAR
	__LDATA *cp;
	int j;
	nschar_t *np;
#endif /* HAVE_WCHAR */

	win->ch_off = win->begx - orig->begx;
	/* Point line pointers to line space. */
	for (lp = win->lspace, i = 0; i < win->maxy; i++, lp++) {
		win->alines[i] = lp;
		olp = orig->alines[i + win->begy - orig->begy];
#ifdef DEBUG
		lp->sentinel = SENTINEL_VALUE;
#endif
		lp->line = &olp->line[win->ch_off];
		lp->firstchp = &olp->firstch;
		lp->lastchp = &olp->lastch;
#ifndef HAVE_WCHAR
		lp->hash = __hash((char *)(void *)lp->line,
		    (size_t) (win->maxx * __LDATASIZE));
#else
		for ( cp = lp->line, j = 0; j < win->maxx; j++, cp++ ) {
			lp->hash = __hash_more( &cp->ch,
				sizeof( wchar_t ), lp->hash );
			lp->hash = __hash_more( &cp->attr,
				sizeof( wchar_t ), lp->hash );
			if ( cp->nsp ) {
				np = cp->nsp;
				while ( np ) {
					lp->hash = __hash_more( &np->ch,
						sizeof( wchar_t ), lp->hash );
					np = np->next;
				}
			}
		}
#endif /* HAVE_WCHAR */
	}

#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "__set_subwin: win->ch_off = %d\n",
	    win->ch_off);
#endif
}
/*
 * __makenew --
 *	Set up a window buffer and returns a pointer to it.
 */
static WINDOW *
__makenew(SCREEN *screen, int nlines, int ncols, int by, int bx, int sub,
	int ispad)
{
	WINDOW			*win;
	__LINE			*lp;
	struct __winlist	*wlp, *wlp2;
	int			 i;


#ifdef	DEBUG
	__CTRACE(__CTRACE_WINDOW, "makenew: (%d, %d, %d, %d)\n",
	    nlines, ncols, by, bx);
#endif
	if (nlines <= 0 || ncols <= 0)
		return NULL;

	if ((win = malloc(sizeof(WINDOW))) == NULL)
		return (NULL);
#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "makenew: win = %p\n", win);
#endif

	/* Set up line pointer array and line space. */
	if ((win->alines = malloc(nlines * sizeof(__LINE *))) == NULL) {
		free(win);
		return NULL;
	}
	if ((win->lspace = malloc(nlines * sizeof(__LINE))) == NULL) {
		free(win->alines);
		free(win);
		return NULL;
	}
	/* Don't allocate window and line space if it's a subwindow */
	if (sub)
		win->wspace = NULL;
	else {
		/*
		 * Allocate window space in one chunk.
		 */
		if ((win->wspace =
			malloc(ncols * nlines * sizeof(__LDATA))) == NULL) {
			free(win->lspace);
			free(win->alines);
			free(win);
			return NULL;
		}
		/*
		 * Append window to window list.
		 */
		if ((wlp = malloc(sizeof(struct __winlist))) == NULL) {
			free(win->wspace);
			free(win->lspace);
			free(win->alines);
			free(win);
			return NULL;
		}
		wlp->winp = win;
		wlp->nextp = NULL;
		if (screen->winlistp == NULL)
			screen->winlistp = wlp;
		else {
			wlp2 = screen->winlistp;
			while (wlp2->nextp != NULL)
				wlp2 = wlp2->nextp;
			wlp2->nextp = wlp;
		}
		/*
		 * Point line pointers to line space, and lines themselves into
		 * window space.
		 */
		for (lp = win->lspace, i = 0; i < nlines; i++, lp++) {
			win->alines[i] = lp;
			lp->line = &win->wspace[i * ncols];
#ifdef DEBUG
			lp->sentinel = SENTINEL_VALUE;
#endif
			lp->firstchp = &lp->firstch;
			lp->lastchp = &lp->lastch;
			if (ispad) {
				lp->firstch = 0;
				lp->lastch = ncols;
			} else {
				lp->firstch = ncols;
				lp->lastch = 0;
			}
		}
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "makenew: ncols = %d\n", ncols);
#endif
	win->screen = screen;
	win->cury = win->curx = 0;
	win->maxy = nlines;
	win->maxx = ncols;
	win->reqy = nlines;
	win->reqx = ncols;

	win->begy = by;
	win->begx = bx;
	win->flags = (__IDLINE | __IDCHAR);
	win->delay = -1;
	win->wattr = 0;
#ifdef HAVE_WCHAR
	win->bnsp = NULL;
	SET_BGWCOL( *win, 1 );
#endif /* HAVE_WCHAR */
	win->scr_t = 0;
	win->scr_b = win->maxy - 1;
	if (ispad) {
		win->flags |= __ISPAD;
		win->pbegy = 0;
		win->pbegx = 0;
		win->sbegy = 0;
		win->sbegx = 0;
		win->smaxy = 0;
		win->smaxx = 0;
	} else
		__swflags(win);
#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "makenew: win->wattr = %08x\n", win->wattr);
	__CTRACE(__CTRACE_WINDOW, "makenew: win->flags = %#.4x\n", win->flags);
	__CTRACE(__CTRACE_WINDOW, "makenew: win->maxy = %d\n", win->maxy);
	__CTRACE(__CTRACE_WINDOW, "makenew: win->maxx = %d\n", win->maxx);
	__CTRACE(__CTRACE_WINDOW, "makenew: win->begy = %d\n", win->begy);
	__CTRACE(__CTRACE_WINDOW, "makenew: win->begx = %d\n", win->begx);
	__CTRACE(__CTRACE_WINDOW, "makenew: win->scr_t = %d\n", win->scr_t);
	__CTRACE(__CTRACE_WINDOW, "makenew: win->scr_b = %d\n", win->scr_b);
#endif
	return (win);
}

void
__swflags(WINDOW *win)
{
	win->flags &= ~(__ENDLINE | __FULLWIN | __SCROLLWIN | __LEAVEOK);
	if (win->begx + win->maxx == COLS && !(win->flags & __ISPAD)) {
		win->flags |= __ENDLINE;
		if (win->begx == 0 && win->maxy == LINES && win->begy == 0)
			win->flags |= __FULLWIN;
		if (win->begy + win->maxy == LINES)
			win->flags |= __SCROLLWIN;
	}
}
