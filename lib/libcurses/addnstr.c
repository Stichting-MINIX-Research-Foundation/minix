/*	$NetBSD: addnstr.c,v 1.13 2012/09/28 06:07:05 blymn Exp $	*/

/*
 * Copyright (c) 1993, 1994
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
static char sccsid[] = "@(#)addnstr.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: addnstr.c,v 1.13 2012/09/28 06:07:05 blymn Exp $");
#endif
#endif				/* not lint */

#include <string.h>

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * addstr --
 *      Add a string to stdscr starting at (_cury, _curx).
 */
int
addstr(const char *s)
{
	return waddnstr(stdscr, s, -1);
}

/*
 * waddstr --
 *      Add a string to the given window starting at (_cury, _curx).
 */
int
waddstr(WINDOW *win, const char *s)
{
	return waddnstr(win, s, -1);
}

/*
 * addnstr --
 *      Add a string (at most n characters) to stdscr starting
 *	at (_cury, _curx).  If n is negative, add the entire string.
 */
int
addnstr(const char *str, int n)
{
	return waddnstr(stdscr, str, n);
}

/*
 * mvaddstr --
 *      Add a string to stdscr starting at (y, x)
 */
int
mvaddstr(int y, int x, const char *str)
{
	return mvwaddnstr(stdscr, y, x, str, -1);
}

/*
 * mvwaddstr --
 *      Add a string to the given window starting at (y, x)
 */
int
mvwaddstr(WINDOW *win, int y, int x, const char *str)
{
	return mvwaddnstr(win, y, x, str, -1);
}

/*
 * mvaddnstr --
 *      Add a string of at most n characters to stdscr
 *      starting at (y, x).
 */
int
mvaddnstr(int y, int x, const char *str, int count)
{
	return mvwaddnstr(stdscr, y, x, str, count);
}

/*
 * mvwaddnstr --
 *      Add a string of at most n characters to the given window
 *      starting at (y, x).
 */
int
mvwaddnstr(WINDOW *win, int y, int x, const char *str, int count)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return waddnstr(win, str, count);
}

#endif

/*
 * waddnstr --
 *	Add a string (at most n characters) to the given window
 *	starting at (_cury, _curx).  If n is negative, add the
 *	entire string.
 */
int
waddnstr(WINDOW *win, const char *s, int n)
{
	size_t  len;
	const char *p;

#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "ADDNSTR: win %p, length %d\n",
			 win, n);
#endif
	/*
	 * behavior changed from traditional BSD curses, for better XCURSES
	 * conformance.
	 *
	 * BSD curses: if (n > 0) then "at most n", else "len = strlen(s)"
	 * ncurses: if (n >= 0) then "at most n", else "len = strlen(s)"
	 * XCURSES: if (n != -1) then "at most n", else "len = strlen(s)"
	 * 
	 * Also SUSv2 says these functions do not wrap nor change the
	 * cursor position.
	 */
	if (n >= 0)
		for (p = s, len = 0; n-- && *p++; ++len);
	else
		len = strlen(s);
	
	if (len > (win->maxx - win->curx))
		len = win->maxx - win->curx;

	return(waddbytes(win, s, (int) len));
}
