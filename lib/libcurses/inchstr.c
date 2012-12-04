/*	$NetBSD: inchstr.c,v 1.6 2012/04/21 11:33:16 blymn Exp $	*/

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
__RCSID("$NetBSD: inchstr.c,v 1.6 2012/04/21 11:33:16 blymn Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * inchstr, inchnstr --
 *	Return an array of characters at cursor position from stdscr.
 */
__warn_references(inchstr,
    "warning: this program uses inchstr(), which is unsafe.")
int
inchstr(chtype *chstr)
{
	return winchstr(stdscr, chstr);
}

int
inchnstr(chtype *chstr, int n)
{
	return winchnstr(stdscr, chstr, n);
}

/*
 * mvinchstr, mvinchnstr --
 *      Return an array of characters at position (y, x) from stdscr.
 */
__warn_references(mvinchstr,
    "warning: this program uses mvinchstr(), which is unsafe.")
int
mvinchstr(int y, int x, chtype *chstr)
{
	return mvwinchstr(stdscr, y, x, chstr);
}

int
mvinchnstr(int y, int x, chtype *chstr, int n)
{
	return mvwinchnstr(stdscr, y, x, chstr, n);
}

/*
 * mvwinchstr, mvwinchnstr --
 *      Return an array characters at position (y, x) from the given window.
 */
__warn_references(mvwinchstr,
    "warning: this program uses mvwinchstr(), which is unsafe.")
int
mvwinchstr(WINDOW *win, int y, int x, chtype *chstr)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return winchstr(win, chstr);
}

int
mvwinchnstr(WINDOW *win, int y, int x, chtype *chstr, int n)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return winchnstr(win, chstr, n);
}

#endif	/* _CURSES_USE_MACROS */

/*
 * winchstr, winchnstr --
 *	Return an array of characters at cursor position.
 */
__warn_references(winchstr,
    "warning: this program uses winchstr(), which is unsafe.")
int
winchstr(WINDOW *win, chtype *chstr)
{

	return winchnstr(win, chstr, -1);
}

/*
 * XXX: This should go in a manpage!
 * - SUSv2/xcurses doesn't document whether the trailing 0 is included
 *   in the length count or not.  For safety's sake it _is_ included.
 */
int
winchnstr(WINDOW *win, chtype *chstr, int n)
{
	__LDATA	*end, *start;
	int epos;

	if (chstr == NULL)
		return ERR;

	start = &win->alines[win->cury]->line[win->curx];
	/* (n - 1) to leave room for the trailing 0 element */
	if (n < 0 || (n - 1) > win->maxx - win->curx - 1)
		epos = win->maxx - 1;
	else
		/* extra -1 for trailing NUL */
		epos = win->curx + n -1 - 1;
	end = &win->alines[win->cury]->line[epos];

	while (start <= end) {
		/* or in the attributes but strip out internal flags */
#ifdef HAVE_WCHAR
		*chstr = start->ch | (start->attr & ~__ACS_IS_WACS);
#else
		*chstr = start->ch | start->attr;
#endif
		chstr++;
		start++;
	}
	*chstr = 0;

	return OK;
}
