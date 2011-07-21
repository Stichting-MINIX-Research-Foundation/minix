/*	$NetBSD: background.c,v 1.15 2009/07/22 16:57:14 roy Exp $	*/

/*-
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
__RCSID("$NetBSD: background.c,v 1.15 2009/07/22 16:57:14 roy Exp $");
#endif				/* not lint */

#include <stdlib.h>
#include "curses.h"
#include "curses_private.h"

/*
 * bkgdset
 *	Set new background attributes on stdscr.
 */
void
bkgdset(chtype ch)
{
	wbkgdset(stdscr, ch);
}

/*
 * bkgd --
 *	Set new background and new background attributes on stdscr.
 */
int
bkgd(chtype ch)
{
	return(wbkgd(stdscr, ch));
}

/*
 * wbkgdset
 *	Set new background attributes.
 */
void
wbkgdset(WINDOW *win, chtype ch)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wbkgdset: (%p), '%s', %08x\n",
	    win, unctrl(ch & +__CHARTEXT), ch & __ATTRIBUTES);
#endif

	/* Background character. */
	if (ch & __CHARTEXT)
		win->bch = (wchar_t) ch & __CHARTEXT;

	/* Background attributes (check colour). */
	if (__using_color && !(ch & __COLOR))
		ch |= __default_color;
	win->battr = (attr_t) ch & __ATTRIBUTES;
}

/*
 * wbkgd --
 *	Set new background and new background attributes.
 */
int
wbkgd(WINDOW *win, chtype ch)
{
	int	y, x;

#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wbkgd: (%p), '%s', %08x\n",
	    win, unctrl(ch & +__CHARTEXT), ch & __ATTRIBUTES);
#endif

	/* Background attributes (check colour). */
	if (__using_color && !(ch & __COLOR))
		ch |= __default_color;

	win->battr = (attr_t) ch & __ATTRIBUTES;
	wbkgdset(win, ch);
	for (y = 0; y < win->maxy; y++)
		for (x = 0; x < win->maxx; x++) {
			/* Copy character if space */
			if (ch & A_CHARTEXT && win->alines[y]->line[x].ch == ' ')
				win->alines[y]->line[x].ch = ch & __CHARTEXT;
			/* Merge attributes */
			if (win->alines[y]->line[x].attr & __ALTCHARSET)
				win->alines[y]->line[x].attr =
				    (ch & __ATTRIBUTES) | __ALTCHARSET;
			else
				win->alines[y]->line[x].attr =
				    ch & __ATTRIBUTES;
#ifdef HAVE_WCHAR
			SET_WCOL(win->alines[y]->line[x], 1);
#endif
		}
	__touchwin(win);
	return(OK);
}

/*
 * getbkgd --
 *	Get current background attributes.
 */
chtype
getbkgd(WINDOW *win)
{
	attr_t	battr;

	/* Background attributes (check colour). */
	battr = win->battr & A_ATTRIBUTES;
	if (__using_color && ((battr & __COLOR) == __default_color))
		battr &= ~__default_color;

	return ((chtype) ((win->bch & A_CHARTEXT) | battr));
}

int bkgrnd(const cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wbkgrnd( stdscr, wch );
#endif /* HAVE_WCHAR */
}

void bkgrndset(const cchar_t *wch)
{
#ifdef HAVE_WCHAR
	wbkgrndset( stdscr, wch );
#endif /* HAVE_WCHAR */
}

int getbkgrnd(cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wgetbkgrnd( stdscr, wch );
#endif /* HAVE_WCHAR */
}

int wbkgrnd(WINDOW *win, const cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
/* 	int	y, x, i; */
	attr_t battr;
/* 	nschar_t *np, *tnp, *pnp; */

#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wbkgrnd: (%p), '%s', %x\n",
		win, (const char *) wunctrl(wch), wch->attributes);
#endif

	/* ignore multi-column characters */
	if ( !wch->elements || wcwidth( wch->vals[ 0 ]) > 1 )
		return ERR;

	/* Background attributes (check colour). */
	battr = wch->attributes & WA_ATTRIBUTES;
	if (__using_color && !( battr & __COLOR))
		battr |= __default_color;

	win->battr = battr;
	wbkgrndset(win, wch);
	__touchwin(win);
	return OK;
#endif /* HAVE_WCHAR */
}

void wbkgrndset(WINDOW *win, const cchar_t *wch)
{
#ifdef HAVE_WCHAR
	attr_t battr;
	nschar_t *np, *tnp;
	int i;

#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wbkgrndset: (%p), '%s', %x\n",
		win, (const char *) wunctrl(wch), wch->attributes);
#endif

	/* ignore multi-column characters */
	if ( !wch->elements || wcwidth( wch->vals[ 0 ]) > 1 )
		return;

	/* Background character. */
	tnp = np = win->bnsp;
	if ( wcwidth( wch->vals[ 0 ]))
		win->bch = wch->vals[ 0 ];
	else {
		if ( !np ) {
			np = (nschar_t *)malloc(sizeof(nschar_t));
			if (!np)
				return;
			np->next = NULL;
			win->bnsp = np;
		}
		np->ch = wch->vals[ 0 ];
		tnp = np;
		np = np->next;
	}
	/* add non-spacing characters */
	if ( wch->elements > 1 ) {
		for ( i = 1; i < wch->elements; i++ ) {
			if ( !np ) {
				np = (nschar_t *)malloc(sizeof(nschar_t));
				if (!np)
					return;
				np->next = NULL;
				if ( tnp )
					tnp->next = np;
				else
					win->bnsp = np;
			}
			np->ch = wch->vals[ i ];
			tnp = np;
			np = np->next;
		}
	}
	/* clear the old non-spacing characters */
	while ( np ) {
		tnp = np->next;
		free( np );
		np = tnp;
	}

	/* Background attributes (check colour). */
	battr = wch->attributes & WA_ATTRIBUTES;
	if (__using_color && !( battr & __COLOR))
		battr |= __default_color;
	win->battr = battr;
	SET_BGWCOL((*win), 1);
#endif /* HAVE_WCHAR */
}

int wgetbkgrnd(WINDOW *win, cchar_t *wch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	nschar_t *np;

	/* Background attributes (check colour). */
	wch->attributes = win->battr & WA_ATTRIBUTES;
	if (__using_color && (( wch->attributes & __COLOR )
			== __default_color))
		wch->attributes &= ~__default_color;
	wch->vals[ 0 ] = win->bch;
	wch->elements = 1;
	np = win->bnsp;
	if (np) {
		while ( np && wch->elements < CURSES_CCHAR_MAX ) {
			wch->vals[ wch->elements++ ] = np->ch;
			np = np->next;
		}
	}

	return OK;
#endif /* HAVE_WCHAR */
}
