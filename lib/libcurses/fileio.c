/*	$NetBSD: fileio.c,v 1.4 2009/07/22 16:57:14 roy Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: fileio.c,v 1.4 2009/07/22 16:57:14 roy Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_WCHAR
static int __putnsp(nschar_t *, FILE *);
static int __getnsp(nschar_t *, FILE *);
#endif /* HAVE_WCHAR */

#ifdef HAVE_WCHAR
/*
 * __putnsp --
 *	Write non-spacing character chain to file, consisting of:
 *	((int) 1, (wchar_t) ch) pairs followed by (int) 0.
 */
static int
__putnsp(nschar_t *nsp, FILE *fp)
{
	int n;

	n = 1;
	while (nsp != NULL) {
		if (fwrite(&n, sizeof(int), 1, fp) != 1)
			return ERR;
		if (fwrite(&nsp->ch, sizeof(wchar_t), 1, fp) != 1)
			return ERR;
	}
	n = 0;
	if (fwrite(&n, sizeof(int), 1, fp) != 1)
		return ERR;

	return OK;
}
#endif /* HAVE_WCHAR */

/*
 * putwin --
 *	Write window data to file
 */
int
putwin(WINDOW *win, FILE *fp)
{
	int major = CURSES_LIB_MAJOR;
	int minor = CURSES_LIB_MINOR;
	int y, x;
	__LDATA *sp;

#ifdef DEBUG
	__CTRACE(__CTRACE_FILEIO, "putwin: win %p\n", win);
#endif

	if (win == NULL)
		return ERR;

	/* win can't be a subwin */
	if (win->orig != NULL)
		return ERR;

	/* Library version */
	if (fwrite(&major, sizeof(int), 1, fp) != 1)
		return ERR;
	if (fwrite(&minor, sizeof(int), 1, fp) != 1)
		return ERR;

	/* Window parameters */
	if (fwrite(win, sizeof(WINDOW), 1, fp) != 1)
		return ERR;
#ifdef HAVE_WCHAR
	/* Background non-spacing character */
	if (__putnsp(win->bnsp, fp) == ERR)
		return ERR;
#endif /* HAVE_WCHAR */

	/* Lines and line data */
	for (y = 0; y < win->maxy; y++)
		for (sp = win->alines[y]->line, x = 0; x < win->maxx;
		    x++, sp++) {
			if (fwrite(&sp->ch, sizeof(wchar_t), 1, fp) != 1)
				return ERR;
			if (fwrite(&sp->attr, sizeof(attr_t), 1, fp) != 1)
				return ERR;
#ifdef HAVE_WCHAR
			if (sp->nsp != NULL) {
				if (__putnsp(win->bnsp, fp) == ERR)
					return ERR;
			}
#endif /* HAVE_WCHAR */
		}

	return OK;
}

#ifdef HAVE_WCHAR
/*
 * __getnsp --
 *	Read non-spacing character chain from file
 */
static int
__getnsp(nschar_t *nsp, FILE *fp)
{
	int n;
	nschar_t *onsp, *tnsp;

	if (fread(&n, sizeof(int), 1, fp) != 1)
		return ERR;
	onsp = nsp;
	while (n != 0) {
		tnsp = (nschar_t *)malloc(sizeof(nschar_t));
		if (tnsp == NULL) {
			__cursesi_free_nsp(nsp);
			return OK;
		}
		if (fread(&tnsp->ch, sizeof(wchar_t), 1, fp) != 1) {
			__cursesi_free_nsp(nsp);
			return OK;
		}
		tnsp->next = NULL;
		onsp->next = tnsp;
		onsp = onsp->next;
		if (fread(&n, sizeof(int), 1, fp) != 1) {
			__cursesi_free_nsp(nsp);
			return ERR;
		}
	}
	return OK;
}
#endif /* HAVE_WCHAR */

/*
 * getwin --
 *	Read window data from file
 */
WINDOW *
getwin(FILE *fp)
{
	int major, minor;
	WINDOW *wtmp, *win;
	int y, x;
	__LDATA *sp;

#ifdef DEBUG
	__CTRACE(__CTRACE_FILEIO, "getwin\n");
#endif

	/* Check library version */
	if (fread(&major, sizeof(int), 1, fp) != 1)
		return NULL;
	if (fread(&minor, sizeof(int), 1, fp) != 1)
		return NULL;
	if(major != CURSES_LIB_MAJOR || minor != CURSES_LIB_MINOR)
		return NULL;

	/* Window parameters */
	wtmp = (WINDOW *)malloc(sizeof(WINDOW));
	if (wtmp == NULL)
		return NULL;
	if (fread(wtmp, sizeof(WINDOW), 1, fp) != 1)
		goto error0;
	win = __newwin(_cursesi_screen, wtmp->maxy, wtmp->maxx,
	    wtmp->begy, wtmp->begx, FALSE);
	if (win == NULL)
		goto error0;
	win->cury = wtmp->cury;
	win->curx = wtmp->curx;
	win->reqy = wtmp->reqy;
	win->reqx = wtmp->reqx;
	win->flags = wtmp->flags;
	win->delay = wtmp->delay;
	win->wattr = wtmp->wattr;
	win->bch = wtmp->bch;
	win->battr = wtmp->battr;
	win->scr_t = wtmp->scr_t;
	win->scr_b = wtmp->scr_b;
	free(wtmp);
	wtmp = NULL;
	__swflags(win);

#ifdef HAVE_WCHAR
	if (__getnsp(win->bnsp, fp) == ERR)
		goto error1;
#endif /* HAVE_WCHAR */

	/* Lines and line data */
	for (y = 0; y < win->maxy; y++) {
		for (sp = win->alines[y]->line, x = 0; x < win->maxx;
		    x++, sp++) {
			if (fread(&sp->ch, sizeof(wchar_t), 1, fp) != 1)
				goto error1;
			if (fread(&sp->attr, sizeof(attr_t), 1, fp) != 1)
				goto error1;
#ifdef HAVE_WCHAR
			if (sp->nsp != NULL) {
				if (__getnsp(win->bnsp, fp) == ERR)
					goto error1;
			}
#endif /* HAVE_WCHAR */
		}
		__touchline(win, y, 0, (int) win->maxx - 1);
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_FILEIO, "getwin: win = 0x%p\n", win);
#endif
	return win;

error1:
	delwin(win);
error0:
	if (wtmp)
		free(wtmp);
	return NULL;
}
