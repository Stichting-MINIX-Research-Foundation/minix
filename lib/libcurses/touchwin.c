/*	$NetBSD: touchwin.c,v 1.27 2013/12/06 11:23:47 blymn Exp $	*/

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
static char sccsid[] = "@(#)touchwin.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: touchwin.c,v 1.27 2013/12/06 11:23:47 blymn Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

static int _cursesi_touchline_force(WINDOW *, int, int, int, int);

/*
 * is_linetouched --
 *	Indicate if line has been touched or not.
 */
bool
is_linetouched(WINDOW *win, int line)
{
	if (line > win->maxy)
		return FALSE;

	return ((win->alines[line]->flags & __ISDIRTY) != 0);
}

/*
 * touchline --
 *	Touch count lines starting at start.  This is the SUS v2 compliant
 *	version.
 */
int
touchline(WINDOW *win, int start, int count)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "touchline: (%p, %d, %d)\n", win, start, count);
#endif
	return wtouchln(win, start, count, 1);
}

/*
 * wredrawln --
 *	Mark count lines starting at start as corrupted.  Implemented using
 *	wtouchln().
 */
int wredrawln(WINDOW *win, int start, int count)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "wredrawln: (%p, %d, %d)\n", win, start, count);
#endif
	return wtouchln(win, start, count, 1);
}

/*
 * is_wintouched --
 *	Check if the window has been touched.
 */
bool
is_wintouched(WINDOW *win)
{
	int y, maxy;

	maxy = win->maxy;
	for (y = 0; y < maxy; y++) {
		if (is_linetouched(win, y) == TRUE)
			return TRUE;
	}

	return FALSE;
}

/*
 * touchwin --
 *	Make it look like the whole window has been changed.
 */
int
touchwin(WINDOW *win)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "touchwin: (%p)\n", win);
#endif
	return wtouchln(win, 0, win->maxy, 1);
}

/*
 * redrawwin --
 *	Mark entire window as corrupted.  Implemented using wtouchln().
 */
int
redrawwin(WINDOW *win)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "redrawwin: (%p)\n", win);
#endif
	return wtouchln(win, 0, win->maxy, 1);
}

/*
 * untouchwin --
 *	 Make it look like the window has not been changed.
 */
int
untouchwin(WINDOW *win)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "untouchwin: (%p)\n", win);
#endif
	return wtouchln(win, 0, win->maxy, 0);
}

/*
 * wtouchln --
 *	If changed is 1 then touch n lines starting at line.  If changed
 *	is 0 then mark the lines as unchanged.
 */
int
wtouchln(WINDOW *win, int line, int n, int changed)
{
	int	y;
	__LINE	*wlp;

#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "wtouchln: (%p) %d, %d, %d\n",
	    win, line, n, changed);
#endif
	if (line + n > win->maxy)
		line = win->maxy - n;
	for (y = line; y < line + n; y++) {
		if (changed == 1)
			_cursesi_touchline_force(win, y, 0,
			    (int) win->maxx - 1, 1);
		else {
			wlp = win->alines[y];
			if (*wlp->firstchp >= win->ch_off &&
			    *wlp->firstchp < win->maxx + win->ch_off)
				*wlp->firstchp = win->maxx + win->ch_off;
			if (*wlp->lastchp >= win->ch_off &&
			    *wlp->lastchp < win->maxx + win->ch_off)
				*wlp->lastchp = win->ch_off;
			wlp->flags &= ~(__ISDIRTY | __ISFORCED);
		}
	}

	return OK;
}

int
__touchwin(WINDOW *win)
{
	int	 y, maxy;

#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "__touchwin: (%p)\n", win);
#endif
	maxy = win->maxy;
	for (y = 0; y < maxy; y++)
		__touchline(win, y, 0, (int) win->maxx - 1);
	return (OK);
}

int
__touchline(WINDOW *win, int y, int sx, int ex)
{
	return (_cursesi_touchline_force(win, y, sx, ex, 0));
}

/*
 * Touch line y on window win starting from column sx and ending at
 * column ex.  If force is 1 then we mark this line as a forced update
 * which will bypass screen optimisation in the refresh code to rewrite
 * this line unconditionally (even if refresh thinks the screen matches
 * what is in the virtscr)
 */
static int
_cursesi_touchline_force(WINDOW *win, int y, int sx, int ex, int force)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "__touchline: (%p, %d, %d, %d)\n",
	    win, y, sx, ex);
	__CTRACE(__CTRACE_LINE, "__touchline: first = %d, last = %d\n",
	    *win->alines[y]->firstchp, *win->alines[y]->lastchp);
#endif
	sx += win->ch_off;
	ex += win->ch_off;
	win->alines[y]->flags |= __ISDIRTY;
	if (force == 1)
		win->alines[y]->flags |= __ISFORCED;
	/* firstchp/lastchp are shared between parent window and sub-window. */
	if (*win->alines[y]->firstchp > sx)
		*win->alines[y]->firstchp = sx;
	if (*win->alines[y]->lastchp < ex)
		*win->alines[y]->lastchp = ex;
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "__touchline: first = %d, last = %d\n",
	    *win->alines[y]->firstchp, *win->alines[y]->lastchp);
#endif
	return (OK);
}

void
wsyncup(WINDOW *win)
{

	do {
		__touchwin(win);
		win = win->orig;
	} while (win);
}

void
wsyncdown(WINDOW *win)
{
	WINDOW *w = win->orig;

	while (w) {
		if (is_wintouched(w)) {
			__touchwin(win);
			break;
		}
		w = w->orig;
	}
}
