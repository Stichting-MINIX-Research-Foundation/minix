/*	$NetBSD: insch.c,v 1.22 2009/07/22 16:57:15 roy Exp $	*/

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
static char sccsid[] = "@(#)insch.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: insch.c,v 1.22 2009/07/22 16:57:15 roy Exp $");
#endif
#endif				/* not lint */

#include <string.h>
#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * insch --
 *	Do an insert-char on the line, leaving (cury, curx) unchanged.
 */
int
insch(chtype ch)
{
	return winsch(stdscr, ch);
}

/*
 * mvinsch --
 *	Do an insert-char on the line at (y, x).
 */
int
mvinsch(int y, int x, chtype ch)
{
	return mvwinsch(stdscr, y, x, ch);
}

/*
 * mvwinsch --
 *	Do an insert-char on the line at (y, x) in the given window.
 */
int
mvwinsch(WINDOW *win, int y, int x, chtype ch)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return winsch(stdscr, ch);
}

#endif

/*
 * winsch --
 *	Do an insert-char on the line, leaving (cury, curx) unchanged.
 */
int
winsch(WINDOW *win, chtype ch)
{

	__LDATA	*end, *temp1, *temp2;
	attr_t attr;

	if (__using_color)
		attr = win->battr & __COLOR;
	else
		attr = 0;
	end = &win->alines[win->cury]->line[win->curx];
	temp1 = &win->alines[win->cury]->line[win->maxx - 1];
	temp2 = temp1 - 1;
	while (temp1 > end) {
		(void) memcpy(temp1, temp2, sizeof(__LDATA));
		temp1--, temp2--;
	}
	temp1->ch = (wchar_t) ch & __CHARTEXT;
	if (temp1->ch == ' ')
		temp1->ch = win->bch;
	temp1->attr = (attr_t) ch & __ATTRIBUTES;
	if (temp1->attr & __COLOR)
		temp1->attr |= (win->battr & ~__COLOR);
	else
		temp1->attr |= win->battr;
#ifdef HAVE_WCHAR
	if (_cursesi_copy_nsp(win->bnsp, temp1) == ERR)
		return ERR;
	SET_WCOL(*temp1, 1);
#endif /* HAVE_WCHAR */
	__touchline(win, (int) win->cury, (int) win->curx, (int) win->maxx - 1);
	if (win->cury == LINES - 1 &&
	    (win->alines[LINES - 1]->line[COLS - 1].ch != ' ' ||
		win->alines[LINES - 1]->line[COLS - 1].attr != attr)) {
		if (win->flags & __SCROLLOK) {
			wrefresh(win);
			scroll(win);
			win->cury--;
		} else
			return (ERR);
	}
	return (OK);
}
