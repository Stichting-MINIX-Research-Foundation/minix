/*	$NetBSD: idcok.c,v 1.1 2002/07/19 13:22:41 blymn Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn (blymn@baea.com.au, brett_lymn@yahoo.com)
 * All rights reserved.
 *
 * This software has been donated to the NetBSD Foundation by the
 * Author.
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
#if 0
static char sccsid[] = "@(#)idcok.c	blymn 2002/06/06";
#else
__RCSID("$NetBSD: idcok.c,v 1.1 2002/07/19 13:22:41 blymn Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * idcok --
 *	Turn on and off using insert/delete char sequences for the
 *	given window.  Note, currently, the ic/dc capabilities are not
 *      used, this function is only provided for compatibility.
 */
int
idcok(WINDOW *win, bool bf)
{
	if (bf)
		win->flags |= __IDCHAR;
	else
		win->flags &= ~__IDCHAR;
	return (OK);
}
