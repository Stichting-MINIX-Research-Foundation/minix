/*	$NetBSD: box.c,v 1.14 2007/05/28 15:01:54 blymn Exp $	*/

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
static char sccsid[] = "@(#)box.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: box.c,v 1.14 2007/05/28 15:01:54 blymn Exp $");
#endif
#endif				/* not lint */

#include "curses.h"

/*
 * box --
 *	Draw a box around the given window with "vert" as the vertical
 *	delimiting char, and "hor", as the horizontal one.  Uses wborder().
 */
int
box(WINDOW *win, chtype vert, chtype hor)
{
	return (wborder(win, vert, vert, hor, hor, 0, 0, 0, 0));
}

int 
box_set(WINDOW *win, const cchar_t *verch, const cchar_t *horch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wborder_set(win, verch, verch, horch, horch, NULL, NULL, NULL, NULL);
#endif /* HAVE_WCHAR */
}
