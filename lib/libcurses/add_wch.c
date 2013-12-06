/*   $NetBSD: add_wch.c,v 1.4 2013/11/09 11:16:59 blymn Exp $ */

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
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__RCSID("$NetBSD: add_wch.c,v 1.4 2013/11/09 11:16:59 blymn Exp $");
#endif /* not lint */

#include <stdlib.h>
#include "curses.h"
#include "curses_private.h"
#ifdef DEBUG
#include <assert.h>
#endif

/*
 * add_wch --
 *	Add the wide character to the current position in stdscr.
 *
 */
int
add_wch(const cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wadd_wch(stdscr, wch);
#endif /* HAVE_WCHAR */
}


/*
 * mvadd_wch --
 *      Add the wide character to stdscr at the given location.
 */
int
mvadd_wch(int y, int x, const cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwadd_wch(stdscr, y, x, wch);
#endif /* HAVE_WCHAR */
}


/*
 * mvwadd_wch --
 *      Add the character to the given window at the given location.
 */
int
mvwadd_wch(WINDOW *win, int y, int x, const cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wadd_wch(win, wch);
#endif /* HAVE_WCHAR */
}


/*
 * wadd_wch --
 *	Add the wide character to the current position in the
 *	given window.
 *
 */
int
wadd_wch(WINDOW *win, const cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	int x = win->curx, y = win->cury;
	__LINE *lnp = NULL;

#ifdef DEBUG
	int i;

	for (i = 0; i < win->maxy; i++) {
		assert(win->alines[i]->sentinel == SENTINEL_VALUE);
	}
	__CTRACE(__CTRACE_INPUT, "wadd_wch: win(%p)", win);
#endif
	lnp = win->alines[y];
	return _cursesi_addwchar(win, &lnp, &y, &x, wch, 1);
#endif /* HAVE_WCHAR */
}
