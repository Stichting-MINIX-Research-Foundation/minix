/*	$NetBSD: instr.c,v 1.3 2009/07/22 16:57:15 roy Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Simon Burge for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL WASABI SYSTEMS, INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: instr.c,v 1.3 2009/07/22 16:57:15 roy Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * instr, innstr --
 *	Return a string of characters at cursor position from stdscr.
 */
__warn_references(instr,
    "warning: this program uses instr(), which is unsafe.")
int
instr(char *str)
{
	return winstr(stdscr, str);
}

int
innstr(char *str, int n)
{
	return winnstr(stdscr, str, n);
}

/*
 * mvinstr, mvinnstr --
 *      Return a string of characters at position (y, x) from stdscr.
 *	XXX: should be multi-byte characters for SUSv2.
 */
__warn_references(mvinstr,
    "warning: this program uses mvinstr(), which is unsafe.")
int
mvinstr(int y, int x, char *str)
{
	return mvwinstr(stdscr, y, x, str);
}

int
mvinnstr(int y, int x, char *str, int n)
{
	return mvwinnstr(stdscr, y, x, str, n);
}

/*
 * mvwinstr, mvwinnstr --
 *      Return an array characters at position (y, x) from the given window.
 *	XXX: should be multi-byte characters for SUSv2.
 */
__warn_references(mvwinstr,
    "warning: this program uses mvwinstr(), which is unsafe.")
int
mvwinstr(WINDOW *win, int y, int x, char *str)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return winstr(win, str);
}

int
mvwinnstr(WINDOW *win, int y, int x, char *str, int n)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return winnstr(win, str, n);
}

#endif	/* _CURSES_USE_MACROS */

/*
 * winstr, winnstr --
 *	Return a string of characters at cursor position.
 *	XXX: should be multi-byte characters for SUSv2.
 */
__warn_references(winstr,
    "warning: this program uses winstr(), which is unsafe.")
int
winstr(WINDOW *win, char *str)
{

	return winnstr(win, str, -1);
}

/*
 * XXX: This should go in a manpage!
 * - winnstr() returns the number of characters copied only of if it is
 *   called with n >= 0 (ie, as inchnstr(), mvinchnstr(), mvwinchnstr()
 *   or winchnstr()).  If N < 0, it returns `OK'.
 * - SUSv2/xcurses doesn't document whether the trailing NUL is included
 *   in the length count or not.  For safety's sake it _is_ included.
 * - This implementation does not (yet) support multi-byte characters
 *   strings.
 */
int
winnstr(WINDOW *win, char *str, int n)
{
	__LDATA	*end, *start;
	int epos;

	if (str == NULL)
		return ERR;

	start = &win->alines[win->cury]->line[win->curx];
	/* (n - 1) to leave room for the trailing NUL */
	if (n < 0 || (n - 1) > win->maxx - win->curx - 1) {
		epos = win->maxx - 1;
		n = win->maxx - win->curx;
	} else {
		/* extra -1 for trailing NUL */
		epos = win->curx + n - 1 - 1;
		n--;
	}
	end = &win->alines[win->cury]->line[epos];

	while (start <= end) {
		*str = start->ch & __CHARTEXT;
		str++;
		start++;
	}
	*str = '\0';

	if (n < 0)
		return OK;
	else
		return n;
}
