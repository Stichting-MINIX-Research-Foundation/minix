/*	$NetBSD: initscr.c,v 1.29 2007/01/22 21:14:53 jdc Exp $	*/

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
static char sccsid[] = "@(#)initscr.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: initscr.c,v 1.29 2007/01/22 21:14:53 jdc Exp $");
#endif
#endif	/* not lint */

#include <stdlib.h>

#include "curses.h"
#include "curses_private.h"


/*
 * initscr --
 *	Initialize the current and standard screen.
 */
WINDOW *
initscr(void)
{
	const char *sp;

#ifdef DEBUG
	__CTRACE(__CTRACE_INIT, "initscr\n");
#endif

	/*
	 * If My_term is set, or can't find a terminal in the environment,
	 * use Def_term.
	 */
	if (My_term || (sp = getenv("TERM")) == NULL)
		sp = Def_term;

	/* LINTED const castaway; newterm does not modify sp! */
	if ((_cursesi_screen = newterm((char *) sp, stdout, stdin)) == NULL)
		return NULL;

	__echoit = _cursesi_screen->echoit;
        __pfast = _cursesi_screen->pfast;
	__rawmode = _cursesi_screen->rawmode;
	__noqch = _cursesi_screen->noqch;
	COLS = _cursesi_screen->COLS;
	LINES = _cursesi_screen->LINES;
	COLORS = _cursesi_screen->COLORS;
	COLOR_PAIRS = _cursesi_screen->COLOR_PAIRS;
	__GT = _cursesi_screen->GT;
	__NONL = _cursesi_screen->NONL;
	__UPPERCASE = _cursesi_screen->UPPERCASE;

	set_term(_cursesi_screen);
	wrefresh(curscr);

	return (stdscr);
}
