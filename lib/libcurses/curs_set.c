/*	$NetBSD: curs_set.c,v 1.9 2010/02/03 15:34:40 roy Exp $	*/

/*-
 * Copyright (c) 1998-2000 Brett Lymn
 *                         (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: curs_set.c,v 1.9 2010/02/03 15:34:40 roy Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * curs_set --
 *    Set the visibility of the cursor, 0 means invisible, 1 means normal
 *    visibility and 2 means high visibility.  Return the previous
 *    visibility iff the terminal supports the new visibility otherwise
 *    return ERR.
 */
int
curs_set(int visibility)
{
	int old_one;

	old_one = _cursesi_screen->old_mode;
	switch (visibility) {
		case 0: /* invisible */
			if (cursor_invisible != NULL) {
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "curs_set: invisible\n");
#endif
				_cursesi_screen->old_mode = 0;
				tputs(cursor_invisible, 0, __cputchar);
				return old_one;
			}
			break;

		case 1: /* normal */
			if (cursor_normal != NULL) {
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC, "curs_set: normal\n");
#endif
				_cursesi_screen->old_mode = 1;
				tputs(cursor_normal, 0, __cputchar);
				return old_one;
			}
			break;

		case 2: /* high visibility */
			if (cursor_visible != NULL) {
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "curs_set: high vis\n");
#endif
				_cursesi_screen->old_mode = 2;
				tputs(cursor_visible, 0, __cputchar);
				return old_one;
			}
			break;

		default:
			break;
	}

	return ERR;
}

/*
 * __restore_cursor_vis --
 *     Restore the old cursor visibility.
 */
void
__restore_cursor_vis(void)
{
	curs_set(_cursesi_screen->old_mode);
}

