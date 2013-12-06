/*	$NetBSD: addch.c,v 1.17 2013/11/09 11:16:59 blymn Exp $	*/

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
static char sccsid[] = "@(#)addch.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: addch.c,v 1.17 2013/11/09 11:16:59 blymn Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * addch --
 *	Add the character to the current position in stdscr.
 *
 */
int
addch(chtype ch)
{
	return waddch(stdscr, ch);
}

/*
 * mvaddch --
 *      Add the character to stdscr at the given location.
 */
int
mvaddch(int y, int x, chtype ch)
{
	return mvwaddch(stdscr, y, x, ch);
}

/*
 * mvwaddch --
 *      Add the character to the given window at the given location.
 */
int
mvwaddch(WINDOW *win, int y, int x, chtype ch)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return waddch(win, ch);
}

#endif

/*
 * waddch --
 *	Add the character to the current position in the given window.
 *
 */
int
waddch(WINDOW *win, chtype ch)
{
#ifdef HAVE_WCHAR
	cchar_t cc;
#else
	__LDATA buf;
#endif

#ifdef HAVE_WCHAR
	__cursesi_chtype_to_cchar(ch, &cc);
#else
	buf.ch = (wchar_t) ch & __CHARTEXT;
	buf.attr = (attr_t) ch & __ATTRIBUTES;
#endif

#ifdef DEBUG
#ifdef HAVE_WCHAR
	__CTRACE(__CTRACE_INPUT,
		 "addch: %d : 0x%x (adding char as wide char)\n",
		 cc.vals[0], cc.attributes);
#else
	__CTRACE(__CTRACE_INPUT, "addch: %d : 0x%x\n", buf.ch, buf.attr);
#endif
#endif

#ifdef HAVE_WCHAR
	return (wadd_wch(win, &cc));
#else
	return (__waddch(win, &buf));
#endif
}

int
__waddch(WINDOW *win, __LDATA *dp)
{
	char	buf[2];

	buf[0] = dp->ch;
	buf[1] = '\0';
	return (_cursesi_waddbytes(win, buf, 1, dp->attr, 1));
}
