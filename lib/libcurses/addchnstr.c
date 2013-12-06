/*	$NetBSD: addchnstr.c,v 1.6 2013/11/09 11:16:59 blymn Exp $	*/

/*
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Douwe Kiela (virtus@wanadoo.nl).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: addchnstr.c,v 1.6 2013/11/09 11:16:59 blymn Exp $");
#endif				/* not lint */

#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * addchstr --
 *      Add a string to stdscr starting at (_cury, _curx).
 */
int
addchstr(const chtype *chstr)
{
	return waddchnstr(stdscr, chstr, -1);
}

/*
 * waddchstr --
 *      Add a string to the given window starting at (_cury, _curx).
 */
int
waddchstr(WINDOW *win, const chtype *chstr)
{
	return waddchnstr(win, chstr, -1);
}

/*
 * addchnstr --
 *      Add a string (at most n characters) to stdscr starting
 *	at (_cury, _curx).  If n is negative, add the entire string.
 */
int
addchnstr(const chtype *chstr, int n)
{
	return waddchnstr(stdscr, chstr, n);
}

/*
 * mvaddchstr --
 *      Add a string to stdscr starting at (y, x)
 */
int
mvaddchstr(int y, int x, const chtype *chstr)
{
	return mvwaddchnstr(stdscr, y, x, chstr, -1);
}

/*
 * mvwaddchstr --
 *      Add a string to the given window starting at (y, x)
 */
int
mvwaddchstr(WINDOW *win, int y, int x, const chtype *chstr)
{
	return mvwaddchnstr(win, y, x, chstr, -1);
}

/*
 * mvaddchnstr --
 *      Add a string of at most n characters to stdscr
 *      starting at (y, x).
 */
int
mvaddchnstr(int y, int x, const chtype *chstr, int n)
{
	return mvwaddchnstr(stdscr, y, x, chstr, n);
}

/*
 * mvwaddchnstr --
 *      Add a string of at most n characters to the given window
 *      starting at (y, x).
 */
int
mvwaddchnstr(WINDOW *win, int y, int x, const chtype *chstr, int n)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return waddchnstr(win, chstr, n);
}

#endif

/*
 * waddchnstr --
 *	Add a string (at most n characters) to the given window
 *	starting at (_cury, _curx) until the end of line is reached or
 *      n characters have been added.  If n is negative, add as much
 *	of the string that will fit on the current line.  SUSv2 says
 *      that the addchnstr family does not wrap and strings are truncated
 *      to the RHS of the window.
 */
int
waddchnstr(WINDOW *win, const chtype *chstr, int n)
{
	size_t  len;
	const chtype *chp;
	attr_t	attr;
	char	*ocp, *cp, *start;
	int i, ret, ox, oy;

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "waddchnstr: win = %p, chstr = %p, n = %d\n",
	    win, chstr, n);
#endif

	if (n >= 0)
		for (chp = chstr, len = 0; n-- && *chp++; ++len);
	else
		for (chp = chstr, len = 0; *chp++; ++len);

	/* check if string is too long for current location */
	if (len > (win->maxx - win->curx))
		len = win->maxx - win->curx;

	if ((ocp = malloc(len + 1)) == NULL)
		return ERR;
	chp = chstr;
	cp = ocp;
	start = ocp;
	i = 0;
	attr = (*chp) & __ATTRIBUTES;
	ox = win->curx;
	oy = win->cury;
	while (len) {
		*cp = (*chp) & __CHARTEXT;
		cp++;
		chp++;
		i++;
		len--;
		if (((*chp) & __ATTRIBUTES) != attr) {
			*cp = '\0';
			if (_cursesi_waddbytes(win, start, i, attr, 0) == ERR) {
				free(ocp);
				return ERR;
			}
			attr = (*chp) & __ATTRIBUTES;
			start = cp;
			i = 0;
		}
	}
	*cp = '\0';
	ret = _cursesi_waddbytes(win, start, i, attr, 0);
	free(ocp);
	wmove(win, oy, ox);
	return ret;
}
