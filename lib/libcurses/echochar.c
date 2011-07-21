/*	$NetBSD: echochar.c,v 1.2 2008/04/29 06:53:01 martin Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: echochar.c,v 1.2 2008/04/29 06:53:01 martin Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS
/*
 * echochar --
 *	Echo character and attributes on stdscr and refresh stdscr.
 */
int
echochar(const chtype ch)
{

	return wechochar(stdscr, ch);
}
#endif	/* _CURSES_USE_MACROS */

/*
 * echochar --
 *	Echo character and attributes on "win" and refresh "win".
 */
int
wechochar(WINDOW *win, const chtype ch)
{
	int retval;

	retval = waddch(win, ch);
	if (retval == OK)
		 retval = wrefresh(win);
	return retval;
}

/*
 * pechochar --
 *	Echo character and attributes on "pad" and refresh "pad" at
 *	its previous position on the screen.
 */
int
pechochar(WINDOW *pad, const chtype ch)
{
	int retval;

	retval = waddch(pad, ch);
	if (retval == OK)
		 retval = prefresh(pad, pad->pbegy, pad->pbegx,
		    pad->sbegy, pad->sbegx, pad->smaxy, pad->smaxx);
	return retval;
}
