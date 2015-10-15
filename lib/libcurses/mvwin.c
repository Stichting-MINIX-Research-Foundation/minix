/*	$NetBSD: mvwin.c,v 1.19 2014/02/20 09:42:42 blymn Exp $	*/

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
static char sccsid[] = "@(#)mvwin.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: mvwin.c,v 1.19 2014/02/20 09:42:42 blymn Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * mvderwin --
 *      Move a derived window.  This does not change the physical screen
 * coordinates of the subwin, rather maps the characters in the subwin
 * sized part of the parent window starting at dy, dx into the subwin.
 *
 */
int
mvderwin(WINDOW *win, int dy, int dx)
{
	WINDOW *parent;
	int x, i;
	__LINE *plp;

	if (win == NULL)
		return ERR;

	parent = win->orig;

	if (parent == NULL)
		return ERR;

	if (((win->maxx + dx) > parent->maxx) ||
	    ((win->maxy + dy) > parent->maxy))
		return ERR;

	win->flags |= __ISDERWIN;
	win->derx = dx;
	win->dery = dy;

	x = parent->begx + dx;

	/*
	 * Mark the source area for the derwin as changed so it will be
	 * copied to the destination window on refresh.
	 */
	for (i = 0; i < win->maxy; i++) {
		plp = parent->alines[i + dy];
		plp->flags = __ISDIRTY;
		if (*plp->firstchp > x)
			*plp->firstchp = x;
		if (*plp->lastchp < x + win->maxx)
			*plp->lastchp = x + win->maxx;
#ifdef DEBUG
		__CTRACE(__CTRACE_REFRESH, "mvderwin: firstchp = %d, lastchp = %d\n", *plp->firstchp, *plp->lastchp);
#endif
	}

	return OK;
}

/*
 * mvwin --
 *	Relocate the starting position of a window.
 */
int
mvwin(WINDOW *win, int by, int bx)
{
	WINDOW *orig;
	int     dy, dx;

	if (by < 0 || by + win->maxy > LINES || bx < 0 || bx + win->maxx > COLS)
		return (ERR);
	dy = by - win->begy;
	dx = bx - win->begx;
	orig = win->orig;
	if (orig == NULL) {
		orig = win;
		do {
			win->begy += dy;
			win->begx += dx;
			__swflags(win);
			win = win->nextp;
		} while (win != orig);
	} else {
		if (by < orig->begy || win->maxy + dy > orig->maxy)
			return (ERR);
		if (bx < orig->begx || win->maxx + dx > orig->maxx)
			return (ERR);
		win->begy = by;
		win->begx = bx;
		__swflags(win);
		__set_subwin(orig, win);
	}
	__touchwin(win);
	return (OK);
}
