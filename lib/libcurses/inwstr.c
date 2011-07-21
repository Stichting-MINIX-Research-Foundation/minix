/*   $NetBSD: inwstr.c,v 1.3 2009/07/22 16:57:15 roy Exp $ */

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
__RCSID("$NetBSD: inwstr.c,v 1.3 2009/07/22 16:57:15 roy Exp $");
#endif						  /* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * inwstr, innwstr --
 *	Return a string of wide characters at cursor position from stdscr.
 */
__warn_references(inwstr,
	"warning: this program uses inwstr(), which is unsafe.")
int
inwstr(wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return winwstr(stdscr, wstr);
#endif /* HAVE_WCHAR */
}

int
innwstr(wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return winnwstr(stdscr, wstr, n);
#endif /* HAVE_WCHAR */
}

/*
 * mvinwstr, mvinnwstr --
 *  Return a string of wide characters at position (y, x) from stdscr.
 */
__warn_references(mvinwstr,
	"warning: this program uses mvinwstr(), which is unsafe.")
int
mvinwstr(int y, int x, wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwinwstr(stdscr, y, x, wstr);
#endif /* HAVE_WCHAR */
}

int
mvinnwstr(int y, int x, wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwinnwstr(stdscr, y, x, wstr, n);
#endif /* HAVE_WCHAR */
}

/*
 * mvwinwstr, mvwinnwstr --
 *  Return an array wide characters at position (y, x) from the given window.
 */
__warn_references(mvwinwstr,
	"warning: this program uses mvwinwstr(), which is unsafe.")
int
mvwinwstr(WINDOW *win, int y, int x, wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return winwstr(win, wstr);
#endif /* HAVE_WCHAR */
}

int
mvwinnwstr(WINDOW *win, int y, int x, wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return winnwstr(win, wstr, n);
#endif /* HAVE_WCHAR */
}

/*
 * winwstr, winnwstr --
 *	Return a string of wide characters at cursor position.
 */
__warn_references(winwstr,
	"warning: this program uses winwstr(), which is unsafe.")
int
winwstr(WINDOW *win, wchar_t *wstr)
{
#ifndef HAVE_WCHAR
	return ERR;
#else

	return winnwstr(win, wstr, -1);
#endif /* HAVE_WCHAR */
}

/*
 * - winnwstr() returns the number of characters copied only of if it is
 *   called with n >= 0 (ie, as in_wchnstr(), mvin_wchnstr(), mvwin_wchnstr()
 *   or win_wchnstr()).  If N < 0, it returns `OK'.
 * - SUSv2/xcurses doesn't document whether the trailing NUL is included
 *   in the length count or not.  For safety's sake it _is_ included.
 * - This implementation does not (yet) support multi-byte characters
 *   strings.
 */
int
winnwstr(WINDOW *win, wchar_t *wstr, int n)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	__LDATA	*start;
	int x, cw, cnt;
	wchar_t *wcp;

	if (wstr == NULL)
		return ERR;

	start = &win->alines[win->cury]->line[win->curx];
	x = win->curx;
	cw = WCOL( *start );
	if (cw < 0) {
		start += cw;
		x += cw;
	}
    cnt = 0;
	wcp = wstr;
	/* (n - 1) to leave room for the trailing 0 element */
	while ((x < win->maxx) && ((n < 0) || ((n > 1) && (cnt < n - 1)))) {
		cw = WCOL( *start );
		*wcp = start->ch;
		wcp++;
		cnt++;
		x += cw;
		if ( x < win->maxx ) 
			start += cw;
	}
	*wcp = L'\0';

	if (n < 0)
		return OK;
	else
		return cnt;
#endif /* HAVE_WCHAR */
}
