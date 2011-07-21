/*	$NetBSD: delwin.c,v 1.17 2009/07/22 16:57:14 roy Exp $	*/

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
static char sccsid[] = "@(#)delwin.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: delwin.c,v 1.17 2009/07/22 16:57:14 roy Exp $");
#endif
#endif				/* not lint */

#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"

/*
 * delwin --
 *	Delete a window and release it back to the system.
 */
int
delwin(WINDOW *win)
{
	WINDOW *wp, *np;
	struct __winlist *wl, *pwl;
	SCREEN *screen;

#ifdef DEBUG
	__CTRACE(__CTRACE_WINDOW, "delwin(%p)\n", win);
#endif
	/*
	 * Free any storage used by non-spacing characters in the window.
	 */
#ifdef HAVE_WCHAR
	__cursesi_win_free_nsp(win);
#endif

	if (win->orig == NULL) {
		/*
		 * If we are the original window, delete the space for all
		 * the subwindows and the window space.
		 */
		free(win->wspace);
		wp = win->nextp;
		while (wp != win) {
			np = wp->nextp;
			delwin(wp);
			wp = np;
		}
		/* Remove ourselves from the list of windows on the screen. */
		pwl = NULL;
		screen = win->screen;
		for (wl = screen->winlistp; wl; pwl = wl, wl = wl->nextp) {
			if (wl->winp != win)
				continue;
			if (pwl != NULL)
				pwl->nextp = wl->nextp;
			else
				screen->winlistp = wl->nextp;
			free(wl);
			break;
		}
	} else {
		/*
		 * If we are a subwindow, take ourselves out of the list.
		 * NOTE: if we are a subwindow, the minimum list is orig
		 * followed by this subwindow, so there are always at least
		 * two windows in the list.
		 */
		for (wp = win->nextp; wp->nextp != win; wp = wp->nextp)
			continue;
		wp->nextp = win->nextp;
	}
	free(win->lspace);
	free(win->alines);
	if (win == _cursesi_screen->curscr)
		_cursesi_screen->curscr = NULL;
	if (win == _cursesi_screen->stdscr)
		_cursesi_screen->stdscr = NULL;
	if (win == _cursesi_screen->__virtscr)
		_cursesi_screen->__virtscr = NULL;
	free(win);
	return (OK);
}
