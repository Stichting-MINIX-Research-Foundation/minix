/*	$NetBSD: move.c,v 1.17 2010/02/23 19:48:26 drochner Exp $	*/

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
static char sccsid[] = "@(#)move.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: move.c,v 1.17 2010/02/23 19:48:26 drochner Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS
/*
 * move --
 *	Moves the cursor to the given point on stdscr.
 */
int
move(int y, int x)
{
	return wmove(stdscr, y, x);
}

#endif

/*
 * wmove --
 *	Moves the cursor to the given point.
 */
int
wmove(WINDOW *win, int y, int x)
{

#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "wmove: (%d, %d)\n", y, x);
#endif
	if (x < 0 || y < 0)
		return (ERR);
	if (x >= win->maxx || y >= win->maxy)
		return (ERR);
	win->curx = x;
	win->alines[win->cury]->flags &= ~__ISPASTEOL;
	win->cury = y;
	win->alines[y]->flags &= ~__ISPASTEOL;
	return (OK);
}

void
wcursyncup(WINDOW *win)
{

	while (win->orig) {
		wmove(win->orig, win->cury + win->begy - win->orig->begy,
			win->curx + win->begx - win->orig->begx);
		win = win->orig;
	}
}
