/*   $NetBSD: in_wch.c,v 1.3 2009/07/22 16:57:14 roy Exp $ */

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
__RCSID("$NetBSD: in_wch.c,v 1.3 2009/07/22 16:57:14 roy Exp $");
#endif						  /* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * in_wch --
 *	Return wide character at cursor position from stdscr.
 */
int
in_wch(cchar_t *wcval)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return win_wch(stdscr, wcval);
#endif /* HAVE_WCHAR */
}

/*
 * mvin_wch --
 *  Return wide character at position (y, x) from stdscr.
 */
int
mvin_wch(int y, int x, cchar_t *wcval)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwin_wch(stdscr, y, x, wcval);
#endif /* HAVE_WCHAR */
}

/*
 * mvwin_wch --
 *  Return wide character at position (y, x) from the given window.
 */
int
mvwin_wch(WINDOW *win, int y, int x, cchar_t *wcval)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return win_wch(win, wcval);
#endif /* HAVE_WCHAR */
}

/*
 * win_wch --
 *	Return wide character at cursor position.
 */
int
win_wch(WINDOW *win, cchar_t *wcval)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	nschar_t *np;
	__LDATA *lp = &win->alines[ win->cury ]->line[ win->curx ];
	int cw = WCOL( *lp );

	if ( cw < 0 ) 
		lp += cw;
	wcval->vals[ 0 ] = lp->ch;
	wcval->attributes = lp->attr;
	wcval->elements = 1;
	np = lp->nsp;
	if (np) {
		do {
			wcval->vals[ wcval->elements++ ] = np->ch;
			np = np-> next;
		} while ( np );
	}

	return OK;
#endif /* HAVE_WCHAR */
}
