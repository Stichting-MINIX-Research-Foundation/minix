/*	$NetBSD: chgat.c,v 1.5 2012/09/28 06:05:19 blymn Exp $	*/

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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
__RCSID("$NetBSD: chgat.c,v 1.5 2012/09/28 06:05:19 blymn Exp $");

#include "curses.h"
#include "curses_private.h"

int
chgat(int n, attr_t attr, short color, const void *opts)
{
	return wchgat(stdscr, n, attr, color, opts);
}

int
mvchgat(int y, int x, int n, attr_t attr, short color,
    const void *opts)
{
	return mvwchgat(stdscr, y, x, n, attr, color, opts);
}

int
wchgat(WINDOW *win, int n, attr_t attr, short color, const void *opts)
{
	return mvwchgat(win, win->cury, win->curx, n, attr, color, opts);
}

int
mvwchgat(WINDOW *win , int y, int x, int count, attr_t attr, short color,
    const void *opts)
{
	__LINE *lp;
	__LDATA *lc;

	if (x < 0 || y < 0)
		return (ERR);
	if (x >= win->maxx || y >= win->maxy)
		return (ERR);

	attr = (attr & ~__COLOR) | COLOR_PAIR(color);

	if (count < 0 || count > win->maxx - x)
		count = win->maxx - x;

#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "mvwchgat: x: %d y: %d count: %d attr: 0x%x "
		 "color pair %d\n", x, y, count, (attr & ~__COLOR),
		 PAIR_NUMBER(color));
#endif
	lp = win->alines[y];
	lc = &lp->line[x];

	if (x + win->ch_off < *lp->firstchp)
		*lp->firstchp = x + win->ch_off;
	if (x + win->ch_off + count > *lp->lastchp)
		*lp->lastchp = x + win->ch_off + count;

	while (count-- > 0) {
		lp->flags |= __ISDIRTY;
#ifdef HAVE_WCHAR
		lc->attr = (lc->attr & ~WA_ATTRIBUTES) | attr;
#else
		lc->attr = attr;
#endif
		++lc;
	}

	return OK;
}
