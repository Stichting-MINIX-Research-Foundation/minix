/*	$NetBSD: clrtobot.c,v 1.22 2012/02/19 19:38:13 christos Exp $	*/

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
static char sccsid[] = "@(#)clrtobot.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: clrtobot.c,v 1.22 2012/02/19 19:38:13 christos Exp $");
#endif
#endif				/* not lint */

#include <stdlib.h>
#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * clrtobot --
 *	Erase everything on stdscr.
 */
int
clrtobot(void)
{
	return wclrtobot(stdscr);
}

#endif

/*
 * wclrtobot --
 *	Erase everything on the window.
 */
int
wclrtobot(WINDOW *win)
{
	int	 minx, startx, starty, y;
	__LDATA	*sp, *end, *maxx;
	attr_t	attr;

#ifdef __GNUC__
	maxx = NULL;		/* XXX gcc -Wuninitialized */
#endif
	if (win->alines[win->cury]->flags & __ISPASTEOL) {
		starty = win->cury + 1;
		startx = 0;
	} else {
		starty = win->cury;
		startx = win->curx;
	}
	if (win != curscr)
		attr = win->battr & __ATTRIBUTES;
	else
		attr = 0;
	for (y = starty; y < win->maxy; y++) {
		minx = -1;
		end = &win->alines[y]->line[win->maxx];
		for (sp = &win->alines[y]->line[startx]; sp < end; sp++) {
#ifndef HAVE_WCHAR
			if (sp->ch != win->bch || sp->attr != attr) {
#else
			if (sp->ch != (wchar_t)btowc((int) win->bch) ||
			    (sp->attr & WA_ATTRIBUTES) != attr || sp->nsp) {
#endif /* HAVE_WCHAR */
				maxx = sp;
				if (minx == -1)
					minx = (int)(sp - win->alines[y]->line);
				if (sp->attr & __ALTCHARSET)
					sp->attr = attr | __ALTCHARSET;
				else
					sp->attr = attr;
#ifdef HAVE_WCHAR
				sp->ch = ( wchar_t )btowc(( int ) win->bch);
				if (_cursesi_copy_nsp(win->bnsp, sp) == ERR)
					return ERR;
				SET_WCOL( *sp, 1 );
#else
				sp->ch = win->bch;
#endif /* HAVE_WCHAR */
			}
		}

		if (minx != -1)
			__touchline(win, y, minx,
				    (int) (maxx - win->alines[y]->line));
		startx = 0;
	}
	return (OK);
}
