/*	$NetBSD: scroll.c,v 1.22 2010/02/03 15:34:40 roy Exp $	*/

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
static char sccsid[] = "@(#)scroll.c	8.3 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: scroll.c,v 1.22 2010/02/03 15:34:40 roy Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * scroll --
 *	Scroll the window up a line.
 */
int
scroll(WINDOW *win)
{
	return(wscrl(win, 1));
}

#ifndef _CURSES_USE_MACROS

/*
 * scrl --
 *	Scroll stdscr n lines - up if n is positive, down if n is negative.
 */
int
scrl(int nlines)
{
	return wscrl(stdscr, nlines);
}

/*
 * setscrreg --
 *	Set the top and bottom of the scrolling region for stdscr.
 */
int
setscrreg(int top, int bottom)
{
	return wsetscrreg(stdscr, top, bottom);
}

#endif

/*
 * wscrl --
 *	Scroll a window n lines - up if n is positive, down if n is negative.
 */
int
wscrl(WINDOW *win, int nlines)
{
	int     oy, ox;

#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "wscrl: (%p) lines=%d\n", win, nlines);
#endif

	if (!(win->flags & __SCROLLOK))
		return (ERR);
	if (!nlines)
		return (OK);

	getyx(win, oy, ox);
#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "wscrl: y=%d\n", oy);
#endif
	if (oy < win->scr_t || oy > win->scr_b)
		/* Outside scrolling region */
		wmove(win, 0, 0);
	else
		/* Inside scrolling region */
		wmove(win, win->scr_t, 0);
	winsdelln(win, 0 - nlines);
	wmove(win, oy, ox);

	if (win == curscr) {
		__cputchar('\n');
		if (!__NONL)
			win->curx = 0;
#ifdef DEBUG
		__CTRACE(__CTRACE_WINDOW, "scroll: win == curscr\n");
#endif
	}
	return (OK);
}

/*
 * wsetscrreg --
 *	Set the top and bottom of the scrolling region for win.
 */
int
wsetscrreg(WINDOW *win, int top, int bottom)
{
	if (top < 0 || bottom >= win->maxy || bottom - top < 1)
		return (ERR);
	win->scr_t = top;
	win->scr_b = bottom;
	return (OK);
}

/*
 * has_ic --
 *	Does the terminal have insert- and delete-character?
 */
bool
has_ic(void)
{
	if (insert_character != NULL && delete_character != NULL)
		return (TRUE);
	else
		return (FALSE);
}

/*
 * has_ic --
 *	Does the terminal have insert- and delete-line?
 */
bool
has_il(void)
{
	if (insert_line !=NULL && delete_line != NULL)
		return (TRUE);
	else
		return (FALSE);
}
