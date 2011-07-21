/*	$NetBSD: delch.c,v 1.22 2009/07/22 16:57:14 roy Exp $	*/

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
static char sccsid[] = "@(#)delch.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: delch.c,v 1.22 2009/07/22 16:57:14 roy Exp $");
#endif
#endif				/* not lint */

#include <string.h>
#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS
/*
 * delch --
 *	Do a delete-char on the line, leaving (cury, curx) unchanged.
 */
int
delch(void)
{
	return wdelch(stdscr);
}

/*
 * mvdelch --
 *	Do a delete-char on the line at (y, x) on stdscr.
 */
int
mvdelch(int y, int x)
{
	return mvwdelch(stdscr, y, x);
}

/*
 * mvwdelch --
 *	Do a delete-char on the line at (y, x) of the given window.
 */
int
mvwdelch(WINDOW *win, int y, int x)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wdelch(win);
}

#endif

/*
 * wdelch --
 *	Do a delete-char on the line, leaving (cury, curx) unchanged.
 */
int
wdelch(WINDOW *win)
{
	__LDATA *end, *temp1, *temp2;

#ifndef HAVE_WCHAR
	end = &win->alines[win->cury]->line[win->maxx - 1];
	temp1 = &win->alines[win->cury]->line[win->curx];
	temp2 = temp1 + 1;
	while (temp1 < end) {
		(void) memcpy(temp1, temp2, sizeof(__LDATA));
		temp1++, temp2++;
	}
	temp1->ch = win->bch;
	if (__using_color && win != curscr)
		temp1->attr = win->battr & __COLOR;
	else
		temp1->attr = 0;
	__touchline(win, (int) win->cury, (int) win->curx, (int) win->maxx - 1);
	return (OK);
#else
	int cw, sx;
	nschar_t *np, *tnp;

	end = &win->alines[win->cury]->line[win->maxx - 1];
	sx = win->curx;
	temp1 = &win->alines[win->cury]->line[win->curx];
	cw = WCOL( *temp1 );
	if ( cw < 0 ) {
		temp1 += cw;
		sx += cw;
		cw = WCOL( *temp1 );
	}
	np = temp1->nsp;
	if (np) {
		while ( np ) {
			tnp = np->next;
			free( np );
			np = tnp;
		}
		temp1->nsp = NULL;
	}
	if ( sx + cw < win->maxx ) {
		temp2 = temp1 + cw;
		while ( temp1 < end - ( cw - 1 )) {
			( void )memcpy( temp1, temp2, sizeof( __LDATA ));
			temp1++, temp2++;
		}
	}
	while ( temp1 <= end ) {
		temp1->ch = ( wchar_t )btowc(( int ) win->bch );
		temp1->attr = 0;
		if (_cursesi_copy_nsp(win->bnsp, temp1) == ERR)
			return ERR;
		SET_WCOL(*temp1, 1);
		temp1++;
	}
	__touchline(win, (int) win->cury, sx, (int) win->maxx - 1);
	return (OK);
#endif /* HAVE_WCHAR */
}
