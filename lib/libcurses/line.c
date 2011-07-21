/*	$NetBSD: line.c,v 1.6 2010/02/23 19:48:26 drochner Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn
 *						 (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: line.c,v 1.6 2010/02/23 19:48:26 drochner Exp $");
#endif				/* not lint */

#include <string.h>

#include "curses.h"
#include "curses_private.h"

/*
 * hline --
 *	Draw a horizontal line of character c on stdscr.
 */
int
hline(chtype ch, int count)
{
	return whline(stdscr, ch, count);
}

/*
 * mvhline --
 *	Move to location (y, x) and draw a horizontal line of character c
 *	on stdscr.
 */
int
mvhline(int y, int x, chtype ch, int count)
{
	return mvwhline(stdscr, y, x, ch, count);
}

/*
 * mvwhline --
 *	Move to location (y, x) and draw a horizontal line of character c
 *	in the given window.
 */
int
mvwhline(WINDOW *win, int y, int x, chtype ch, int count)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return whline(win, ch, count);
}

/*
 * whline --
 *	Draw a horizontal line of character c in the given window moving
 *	towards the rightmost column.  At most count characters are drawn
 *	or until the edge of the screen, whichever comes first.
 */
int
whline(WINDOW *win, chtype ch, int count)
{
#ifndef HAVE_WCHAR
	int ocurx, n, i;

	n = min(count, win->maxx - win->curx);
	ocurx = win->curx;

	if (!(ch & __CHARTEXT))
		ch |= ACS_HLINE;
	for (i = 0; i < n; i++)
		mvwaddch(win, win->cury, ocurx + i, ch);
		
	wmove(win, win->cury, ocurx);
	return OK;
#else
	cchar_t cch, *cchp;

	if (ch & __CHARTEXT) {
		__cursesi_chtype_to_cchar(ch, &cch);
		cchp = & cch;
	} else
		cchp = WACS_HLINE;

	return whline_set(win, cchp, count);
#endif
}

/*
 * vline --
 *	Draw a vertical line of character ch on stdscr.
 */
int
vline(chtype ch, int count)
{
	return wvline(stdscr, ch, count);
}

/*
 * mvvline --
 *	Move to the given location an draw a vertical line of character ch.
 */
int
mvvline(int y, int x, chtype ch, int count)
{
	return mvwvline(stdscr, y, x, ch, count);
}

/*
 * mvwvline --
 *	Move to the given location and draw a vertical line of character ch
 *	on the given window.
 */
int
mvwvline(WINDOW *win, int y, int x, chtype ch, int count)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wvline(win, ch, count);
}

/*
 * wvline --
 *	Draw a vertical line of character ch in the given window moving
 *	towards the bottom of the screen.  At most count characters are drawn
 *	or until the edge of the screen, whichever comes first.
 */
int
wvline(WINDOW *win, chtype ch, int count)
{
#ifndef HAVE_WCHAR
	int ocury, ocurx, n, i;

	n = min(count, win->maxy - win->cury);
	ocury = win->cury;
	ocurx = win->curx;

	if (!(ch & __CHARTEXT))
		ch |= ACS_VLINE;
	for (i = 0; i < n; i++)
		mvwaddch(win, ocury + i, ocurx, ch);

	wmove(win, ocury, ocurx);
	return OK;
#else
	cchar_t cch, *cchp;

	if (ch & __CHARTEXT) {
		__cursesi_chtype_to_cchar(ch, &cch);
		cchp = & cch;
	} else
		cchp = WACS_VLINE;

	return wvline_set(win, cchp, count);
#endif
}

int hline_set(const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return whline_set( stdscr, wch, n );
#endif /* HAVE_WCHAR */
}

int mvhline_set(int y, int x, const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwhline_set( stdscr, y, x, wch, n );
#endif /* HAVE_WCHAR */
}

int mvwhline_set(WINDOW *win, int y, int x, const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if ( wmove( win, y , x ) == ERR )
		return ERR;

	return whline_set( win, wch, n );
#endif /* HAVE_WCHAR */
}

int whline_set(WINDOW *win, const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	int ocurx, wcn, i, cw;
	cchar_t cc;

	cw = wcwidth( wch->vals[ 0 ]);
	if (cw < 0)
		cw = 1;
	if ( ( win->maxx - win->curx ) < cw )
		return ERR;
	wcn = min( n, ( win->maxx - win->curx ) / cw );
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "whline_set: line of %d\n", wcn);
#endif /* DEBUG */
	ocurx = win->curx;

	memcpy( &cc, wch, sizeof( cchar_t ));
	if (!(wch->vals[ 0 ]))
		cc.vals[ 0 ] |= WACS_HLINE->vals[0];
	for (i = 0; i < wcn; i++ ) {
#ifdef DEBUG
		__CTRACE(__CTRACE_LINE, "whline_set: (%d,%d)\n",
		   win->cury, ocurx + i * cw);
#endif /* DEBUG */
		mvwadd_wch(win, win->cury, ocurx + i * cw, &cc);
	}
		
	wmove(win, win->cury, ocurx);
	return OK;
#endif /* HAVE_WCHAR */
}

int vline_set(const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wvline_set( stdscr, wch, n );
#endif /* HAVE_WCHAR */
}

int mvvline_set(int y, int x, const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwvline_set( stdscr, y, x, wch, n );
#endif /* HAVE_WCHAR */
}

int mvwvline_set(WINDOW *win, int y, int x, const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if ( wmove( win, y, x ) == ERR )
		return ERR;

	return wvline_set( win, wch, n );
#endif /* HAVE_WCHAR */
}

int wvline_set(WINDOW *win, const cchar_t *wch, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	int ocury, ocurx, wcn, i;
	cchar_t cc;

	wcn = min( n, win->maxy - win->cury);
#ifdef DEBUG
	__CTRACE(__CTRACE_LINE, "wvline_set: line of %d\n", wcn);
#endif /* DEBUG */
	ocury = win->cury;
	ocurx = win->curx;

	memcpy( &cc, wch, sizeof( cchar_t ));
	if (!(wch->vals[ 0 ]))
		cc.vals[ 0 ] |= WACS_VLINE->vals[0];
	for (i = 0; i < wcn; i++) {
		mvwadd_wch(win, ocury + i, ocurx, &cc);
#ifdef DEBUG
		__CTRACE(__CTRACE_LINE, "wvline_set: (%d,%d)\n",
		    ocury + i, ocurx);
#endif /* DEBUG */
	}
	wmove(win, ocury, ocurx);
	return OK;
#endif /* HAVE_WCHAR */
}
