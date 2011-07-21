/*   $NetBSD: get_wstr.c,v 1.3 2008/04/14 20:33:59 jdc Exp $ */

/*
 * Copyright (c) 2005 The NetBSD Foundation Inc.
 * All rights reserved.
 *
 * This code is derived from code donated to the NetBSD Foundation
 * by Ruibiao Qiu <ruibiao@arl.wustl.edu,ruibiao@gmail.com>.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the NetBSD Foundation nor the names of its
 *	contributors may be used to endorse or promote products derived
 *	from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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
__RCSID("$NetBSD: get_wstr.c,v 1.3 2008/04/14 20:33:59 jdc Exp $");
#endif						  /* not lint */

#include "curses.h"
#include "curses_private.h"

/* prototypes for private functions */
#ifdef HAVE_WCHAR
static int __wgetn_wstr(WINDOW *, wchar_t *, int);
#endif /* HAVE_WCHAR */

/*
 * getn_wstr --
 *	Get a string (of maximum n) characters from stdscr starting at
 *	(cury, curx).
 */
int
getn_wstr(wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wgetn_wstr(stdscr, wstr, n);
#endif /* HAVE_WCHAR */
}

/*
 * get_wstr --
 *	Get a string from stdscr starting at (cury, curx).
 */
__warn_references(get_wstr,
	"warning: this program uses get_wstr(), which is unsafe.")
int
get_wstr(wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wget_wstr(stdscr, wstr);
#endif /* HAVE_WCHAR */
}

/*
 * mvgetn_wstr --
 *  Get a string (of maximum n) characters from stdscr starting at (y, x).
 */
int
mvgetn_wstr(int y, int x, wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwgetn_wstr(stdscr, y, x, wstr, n);
#endif /* HAVE_WCHAR */
}

/*
 * mvget_wstr --
 *	  Get a string from stdscr starting at (y, x).
 */
__warn_references(mvget_wstr,
	"warning: this program uses mvget_wstr(), which is unsafe.")
int
mvget_wstr(int y, int x, wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwget_wstr(stdscr, y, x, wstr);
#endif /* HAVE_WCHAR */
}

/*
 * mvwgetn_wstr --
 *  Get a string (of maximum n) characters from the given window starting
 *	at (y, x).
 */
int
mvwgetn_wstr(WINDOW *win, int y, int x, wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wgetn_wstr(win, wstr, n);
#endif /* HAVE_WCHAR */
}

/*
 * mvwget_wstr --
 *	  Get a string from the given window starting at (y, x).
 */
__warn_references(mvget_wstr,
	"warning: this program uses mvget_wstr(), which is unsafe.")
int
mvwget_wstr(WINDOW *win, int y, int x, wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wget_wstr(win, wstr);
#endif /* HAVE_WCHAR */
}

/*
 * wget_wstr --
 *	Get a string starting at (cury, curx).
 */
__warn_references(wget_wstr,
	"warning: this program uses wget_wstr(), which is unsafe.")
int
wget_wstr(WINDOW *win, wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return __wgetn_wstr(win, wstr, -1);
#endif /* HAVE_WCHAR */
}

/*
 * wgetn_wstr --
 *	Get a string starting at (cury, curx).
 *	Note that n <  2 means that we return ERR (SUSv2 specification).
 */
int
wgetn_wstr(WINDOW *win, wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (n < 1)
		return (ERR);
	if (n == 1) {
		wstr[0] = L'\0';
		return (ERR);
	}
	return __wgetn_wstr(win, wstr, n);
#endif /* HAVE_WCHAR */
}

#ifdef HAVE_WCHAR
/*
 * __wgetn_wstr --
 *	The actual implementation.
 *	Note that we include a trailing L'\0' for safety, so str will contain
 *	at most n - 1 other characters.
 */
int
__wgetn_wstr(WINDOW *win, wchar_t *wstr, int n)
{
	wchar_t *ostr, ec, kc, sc[ 2 ];
	int oldx, remain;
	wint_t wc;
	cchar_t cc;

	ostr = wstr;
	if ( erasewchar( &ec ) == ERR )
		return ERR;
	if ( killwchar( &kc ) == ERR )
		return ERR;
	sc[ 0 ] = ( wchar_t )btowc( ' ' );
	sc[ 1 ] = L'\0';
	setcchar( &cc, sc, win->wattr, 0, NULL );
	oldx = win->curx;
	remain = n - 1;

	while (wget_wch(win, &wc) != ERR
	       && wc != L'\n' && wc != L'\r') {
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT,
		    "__wgetn_wstr: win %p, char 0x%x, remain %d\n",
		    win, wc, remain);
#endif
		*wstr = wc;
		touchline(win, win->cury, 1);
		if (wc == ec || wc == KEY_BACKSPACE || wc == KEY_LEFT) {
			*wstr = L'\0';
			if (wstr != ostr) {
				if ((wchar_t)wc == ec) {
					mvwadd_wch(win, win->cury,
						win->curx, &cc);
					wmove(win, win->cury, win->curx - 1);
				}
				if (wc == KEY_BACKSPACE || wc == KEY_LEFT) {
					/* getch() displays the key sequence */
					mvwadd_wch(win, win->cury,
						win->curx - 1, &cc);
					mvwadd_wch(win, win->cury,
						win->curx - 2, &cc);
					wmove(win, win->cury, win->curx - 1);
				}
				wstr--;
				if (n != -1) {
					/* We're counting chars */
					remain++;
				}
			} else { /* str == ostr */
				if (wc == KEY_BACKSPACE || wc == KEY_LEFT)
					/* getch() displays the other keys */
					mvwadd_wch(win, win->cury,
						win->curx - 1, &cc);
				wmove(win, win->cury, oldx);
			}
		} else if (wc == kc) {
			*wstr = L'\0';
			if (wstr != ostr) {
				/* getch() displays the kill character */
				mvwadd_wch(win, win->cury, win->curx - 1, &cc);
				/* Clear the characters from screen and str */
				while (wstr != ostr) {
					mvwadd_wch(win, win->cury,
						win->curx - 1, &cc);
					wmove(win, win->cury, win->curx - 1);
					wstr--;
					if (n != -1)
						/* We're counting chars */
						remain++;
				}
				mvwadd_wch(win, win->cury, win->curx - 1, &cc);
				wmove(win, win->cury, win->curx - 1);
			} else
				/* getch() displays the kill character */
				mvwadd_wch( win, win->cury, oldx, &cc );
			wmove(win, win->cury, oldx);
		} else if (wc >= KEY_MIN && wc <= KEY_MAX) {
			/* get_wch() displays these characters */
			mvwadd_wch( win, win->cury, win->curx - 1, &cc );
			wmove(win, win->cury, win->curx - 1);
		} else {
			if (remain) {
				wstr++;
				remain--;
			} else {
				mvwadd_wch(win, win->cury, win->curx - 1, &cc);
				wmove(win, win->cury, win->curx - 1);
			}
		}
	}

	if (wc == ERR) {
		*wstr = L'\0';
		return ERR;
	}
	*wstr = L'\0';
	return OK;
}
#endif /* HAVE_WCHAR */
