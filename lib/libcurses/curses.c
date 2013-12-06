/*	$NetBSD: curses.c,v 1.25 2013/10/16 19:59:29 roy Exp $	*/

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
#include <stdlib.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)curses.c	8.3 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: curses.c,v 1.25 2013/10/16 19:59:29 roy Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/* Private. */
int	__echoit = 1;			 /* If stty indicates ECHO. */
int	__pfast;
int	__rawmode = 0;			 /* If stty indicates RAW mode. */
int	__noqch = 0;
					 /* If terminal doesn't have
					 * insert/delete line capabilities
					 * for quick change on refresh.
					 */
char	__CA;

/*
 * Public.
 *
 * XXX
 * UPPERCASE isn't used by libcurses, and is left for backward
 * compatibility only.
 */
WINDOW	*curscr;			/* Current screen. */
WINDOW	*stdscr;			/* Standard screen. */
WINDOW	*__virtscr;			/* Virtual screen (for doupdate()). */
SCREEN  *_cursesi_screen;               /* the current screen we are using */
int	 COLS;				/* Columns on the screen. */
int	 LINES;				/* Lines on the screen. */
int	 TABSIZE;			/* Size of a tab. */
int	 COLORS;			/* Maximum colors on the screen */
int	 COLOR_PAIRS = 0;		/* Maximum color pairs on the screen */
int	 My_term = 0;			/* Use Def_term regardless. */
const char	*Def_term = "unknown";	/* Default terminal type. */
char	 __GT;				/* Gtty indicates tabs. */
char	 __NONL;			/* Term can't hack LF doing a CR. */
char	 __UPPERCASE;			/* Terminal is uppercase only. */

#ifdef HAVE_WCHAR
/*
 * Copy the non-spacing character list (src_nsp) to the given character,
 * allocate or free storage as required.
 */
int
_cursesi_copy_nsp(nschar_t *src_nsp, struct __ldata *ch)
{
	nschar_t *np, *tnp, *pnp;

	pnp = NULL;
	np = src_nsp;
	if (np) {
		tnp = ch->nsp;
		while (np) {
			if (tnp) {
				tnp->ch = np->ch;
				pnp = tnp;
				tnp = tnp->next;
			} else {
				tnp = (nschar_t *)malloc(sizeof(nschar_t));
				if (!tnp)
					return ERR;
				tnp->ch = np->ch;
				pnp->next = tnp;
				tnp->next = NULL;
				pnp = tnp;
				tnp = NULL;
			}
			np = np->next;
		}
                np = tnp;
		if (np) {
			pnp->next = NULL;
			__cursesi_free_nsp(np);
		}
	} else {
		if (ch->nsp) {
			__cursesi_free_nsp(ch->nsp);
			ch->nsp = NULL;
		}
	}

	return OK;
}

/*
 * Free the storage associated with a non-spacing character - traverse the
 * linked list until all storage is done.
 */
void
__cursesi_free_nsp(nschar_t *inp)
{
	nschar_t *tnp, *np;

	np = inp;
	if (np) {
		while (np) {
			tnp = np->next;
			free(np);
			np = tnp;
		}
	}
}

/*
 * Traverse all the cells in the given window free'ing the non-spacing
 * character storage.
 */
void
__cursesi_win_free_nsp(WINDOW *win)
{
	int     i, j;
	__LDATA *sp;

	for (i = 0; i < win->maxy; i++) {
		for (sp = win->alines[i]->line, j = 0; j < win->maxx;
		     j++, sp++) {
			__cursesi_free_nsp(sp->nsp);
		}
	}
}

#endif
