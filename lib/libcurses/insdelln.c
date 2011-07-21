/*	$NetBSD: insdelln.c,v 1.16 2009/07/22 16:57:15 roy Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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
__RCSID("$NetBSD: insdelln.c,v 1.16 2009/07/22 16:57:15 roy Exp $");
#endif				/* not lint */

/*
 * Based on deleteln.c and insertln.c -
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 */

#include <string.h>
#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * insdelln --
 *	Insert or delete lines on stdscr, leaving (cury, curx) unchanged.
 */
int
insdelln(int nlines)
{
	return winsdelln(stdscr, nlines);
}

#endif

/*
 * winsdelln --
 *	Insert or delete lines on the window, leaving (cury, curx) unchanged.
 */
int
winsdelln(WINDOW *win, int nlines)
{
	int     y, i, last;
	__LINE *temp;
#ifdef HAVE_WCHAR
	__LDATA *lp;
#endif /* HAVE_WCHAR */
	attr_t	attr;

#ifdef DEBUG
	__CTRACE(__CTRACE_LINE,
	    "winsdelln: (%p) cury=%d lines=%d\n", win, win->cury, nlines);
#endif

	if (!nlines)
		return(OK);

	if (__using_color && win != curscr)
		attr = win->battr & __COLOR;
	else
		attr = 0;

	if (nlines > 0) {
		/* Insert lines */
		if (win->cury < win->scr_t || win->cury > win->scr_b) {
			/*  Outside scrolling region */
			if (nlines > win->maxy - win->cury)
				nlines = win->maxy - win->cury;
			last = win->maxy - 1;
		} else {
			/* Inside scrolling region */
			if (nlines > win->scr_b + 1 - win->cury)
				nlines = win->scr_b + 1 - win->cury;
			last = win->scr_b;
		}
		for (y = last - nlines; y >= win->cury; --y) {
			win->alines[y]->flags &= ~__ISPASTEOL;
			win->alines[y + nlines]->flags &= ~__ISPASTEOL;
			if (win->orig == NULL) {
				temp = win->alines[y + nlines];
				win->alines[y + nlines] = win->alines[y];
				win->alines[y] = temp;
			} else {
				(void) memcpy(win->alines[y + nlines]->line,
				    win->alines[y]->line,
				    (size_t) win->maxx * __LDATASIZE);
			}
		}
		for (y = win->cury - 1 + nlines; y >= win->cury; --y)
			for (i = 0; i < win->maxx; i++) {
				win->alines[y]->line[i].ch = win->bch;
				win->alines[y]->line[i].attr = attr;
#ifndef HAVE_WCHAR
				win->alines[y]->line[i].ch = win->bch;
#else
				win->alines[y]->line[i].ch
					= ( wchar_t )btowc(( int ) win->bch );
				lp = &win->alines[y]->line[i];
				if (_cursesi_copy_nsp(win->bnsp, lp) == ERR)
					return ERR;
				SET_WCOL( *lp, 1 );
#endif /* HAVE_WCHAR */
			}
		for (y = last; y >= win->cury; --y)
			__touchline(win, y, 0, (int) win->maxx - 1);
	} else {
		/* Delete nlines */
		nlines = 0 - nlines;
		if (win->cury < win->scr_t || win->cury > win->scr_b) {
			/*  Outside scrolling region */
			if (nlines > win->maxy - win->cury)
				nlines = win->maxy - win->cury;
			last = win->maxy;
		} else {
			/* Inside scrolling region */
			if (nlines > win->scr_b + 1 - win->cury)
				nlines = win->scr_b + 1 - win->cury;
			last = win->scr_b + 1;
		}
		for (y = win->cury; y < last - nlines; y++) {
			win->alines[y]->flags &= ~__ISPASTEOL;
			win->alines[y + nlines]->flags &= ~__ISPASTEOL;
			if (win->orig == NULL) {
				temp = win->alines[y];
				win->alines[y] = win->alines[y + nlines];
				win->alines[y + nlines] = temp;
			} else {
				(void) memcpy(win->alines[y]->line,
				    win->alines[y + nlines]->line,
				    (size_t) win->maxx * __LDATASIZE);
			}
		}
		for (y = last - nlines; y < last; y++)
			for (i = 0; i < win->maxx; i++) {
				win->alines[y]->line[i].ch = win->bch;
				win->alines[y]->line[i].attr = attr;
#ifndef HAVE_WCHAR
				win->alines[y]->line[i].ch = win->bch;
#else
				win->alines[y]->line[i].ch
					= (wchar_t)btowc((int) win->bch);
				lp = &win->alines[y]->line[i];
				SET_WCOL( *lp, 1 );
				if (_cursesi_copy_nsp(win->bnsp, lp) == ERR)
					return ERR;
#endif /* HAVE_WCHAR */
			}
		for (y = win->cury; y < last; y++)
			__touchline(win, y, 0, (int) win->maxx - 1);
	}
	if (win->orig != NULL)
		__id_subwins(win->orig);
	return (OK);
}
