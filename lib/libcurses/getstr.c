/*	$NetBSD: getstr.c,v 1.22 2012/01/27 15:37:09 christos Exp $	*/

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

#include <assert.h>
#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)getstr.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: getstr.c,v 1.22 2012/01/27 15:37:09 christos Exp $");
#endif
#endif				/* not lint */

#include <ctype.h>
#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * getnstr --
 *	Get a string (of maximum n) characters from stdscr starting at
 *	(cury, curx).
 */
int
getnstr(char *str, int n)
{
	return wgetnstr(stdscr, str, n);
}

/*
 * getstr --
 *	Get a string from stdscr starting at (cury, curx).
 */
__warn_references(getstr,
    "warning: this program uses getstr(), which is unsafe.")
int
getstr(char *str)
{
	return wgetstr(stdscr, str);
}

/*
 * mvgetnstr --
 *      Get a string (of maximum n) characters from stdscr starting at (y, x).
 */
int
mvgetnstr(int y, int x, char *str, int n)
{
	return mvwgetnstr(stdscr, y, x, str, n);
}

/*
 * mvgetstr --
 *      Get a string from stdscr starting at (y, x).
 */
__warn_references(mvgetstr,
    "warning: this program uses mvgetstr(), which is unsafe.")
int
mvgetstr(int y, int x, char *str)
{
	return mvwgetstr(stdscr, y, x, str);
}

/*
 * mvwgetnstr --
 *      Get a string (of maximum n) characters from the given window starting
 *	at (y, x).
 */
int
mvwgetnstr(WINDOW *win, int y, int x, char *str, int n)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wgetnstr(win, str, n);
}

/*
 * mvwgetstr --
 *      Get a string from the given window starting at (y, x).
 */
__warn_references(mvgetstr,
    "warning: this program uses mvgetstr(), which is unsafe.")
int
mvwgetstr(WINDOW *win, int y, int x, char *str)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wgetstr(win, str);
}

#endif

/*
 * wgetstr --
 *	Get a string starting at (cury, curx).
 */
__warn_references(wgetstr,
    "warning: this program uses wgetstr(), which is unsafe.")
int
wgetstr(WINDOW *win, char *str)
{
	return __wgetnstr(win, str, -1);
}

/*
 * wgetnstr --
 *	Get a string starting at (cury, curx).
 *	Note that n <  2 means that we return ERR (SUSv2 specification).
 */
int
wgetnstr(WINDOW *win, char *str, int n)
{
	if (n < 1)
		return (ERR);
	if (n == 1) {
		str[0] = '\0';
		return (ERR);
	}
	return __wgetnstr(win, str, n);
}

/*
 * __wgetnstr --
 *	The actual implementation.
 *	Note that we include a trailing '\0' for safety, so str will contain
 *	at most n - 1 other characters.
 *	XXX: character deletion from screen is based on how the characters
 *	are displayed by wgetch().
 */
int
__wgetnstr(WINDOW *win, char *str, int n)
{
	char *ostr, ec, kc;
	int c, xpos, oldx, remain;

	ostr = str;
	ec = erasechar();
	kc = killchar();
	xpos = oldx = win->curx;
	_DIAGASSERT(n == -1 || n > 1);
	remain = n - 1;

	while ((c = wgetch(win)) != ERR && c != '\n' && c != '\r') {
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT,
		    "__wgetnstr: win %p, char 0x%x, remain %d\n",
		    win, c, remain);
#endif
		*str = c;
		touchline(win, win->cury, 1);
		if (c == ec || c == KEY_BACKSPACE || c == KEY_LEFT) {
			*str = '\0';
			if (str != ostr) {
				if ((char) c == ec) {
					mvwaddch(win, win->cury, xpos, ' ');
					if (xpos > oldx)
						mvwaddch(win, win->cury,
						    xpos - 1, ' ');
					if (win->curx > xpos - 1)
						wmove(win, win->cury, xpos - 1);
					xpos--;
				}
				if (c == KEY_BACKSPACE || c == KEY_LEFT) {
					/* getch() displays the key sequence */
					mvwaddch(win, win->cury, win->curx,
					    ' ');
					mvwaddch(win, win->cury, win->curx - 1,
					    ' ');
					if (win->curx > xpos)
						wmove(win, win->cury, xpos - 1);
					xpos--;
				}
				str--;
				if (n != -1) {
					/* We're counting chars */
					remain++;
				}
			} else {        /* str == ostr */
				/* getch() displays the other keys */
				if (win->curx > oldx)
					mvwaddch(win, win->cury, win->curx - 1,
					    ' ');
				wmove(win, win->cury, oldx);
				xpos = oldx;
			}
		} else if (c == kc) {
			*str = '\0';
			if (str != ostr) {
				/* getch() displays the kill character */
				mvwaddch(win, win->cury, win->curx - 1, ' ');
				/* Clear the characters from screen and str */
				while (str != ostr) {
					mvwaddch(win, win->cury, win->curx - 1,
					    ' ');
					wmove(win, win->cury, win->curx - 1);
					str--;
					if (n != -1)
						/* We're counting chars */
						remain++;
				}
				mvwaddch(win, win->cury, win->curx - 1, ' ');
				wmove(win, win->cury, win->curx - 1);
			} else
				/* getch() displays the kill character */
				mvwaddch(win, win->cury, oldx, ' ');
			wmove(win, win->cury, oldx);
		} else if (c >= KEY_MIN && c <= KEY_MAX) {
			/* getch() displays these characters */
			mvwaddch(win, win->cury, xpos, ' ');
			wmove(win, win->cury, xpos);
		} else {
			if (remain) {
				if (iscntrl((unsigned char)c))
					mvwaddch(win, win->cury, xpos, ' ');
				str++;
				xpos++;
				remain--;
			} else
				mvwaddch(win, win->cury, xpos, ' ');
			wmove(win, win->cury, xpos);
		}
	}

	if (c == ERR) {
		*str = '\0';
		return (ERR);
	}
	*str = '\0';
	return (OK);
}
