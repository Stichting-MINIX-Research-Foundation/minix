/*	$NetBSD: standout.c,v 1.16 2010/02/03 15:34:40 roy Exp $	*/

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
static char sccsid[] = "@(#)standout.c	8.3 (Berkeley) 8/10/94";
#else
__RCSID("$NetBSD: standout.c,v 1.16 2010/02/03 15:34:40 roy Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * standout --
 *	Enter standout mode on stdscr.
 */
int
standout(void)
{
	return wstandout(stdscr);
}

/*
 * standend --
 *	Exit standout mode on stdscr.
 */
int
standend(void)
{
	return wstandend(stdscr);
}

#endif

/*
 * wstandout --
 *	Enter standout mode in window win.
 */
int
wstandout(WINDOW *win)
{
	/*
	 * If standout/standend strings, or can underline, set the
	 * screen standout bit.
	 */
	if ((enter_standout_mode != NULL && exit_standout_mode != NULL) ||
	    underline_char != NULL)
		win->wattr |= __STANDOUT;
	return (1);
}

/*
 * wstandend --
 *	Exit standout mode in window win.
 */
int
wstandend(WINDOW *win)
{
	win->wattr &= ~__STANDOUT;
	return (1);
}
