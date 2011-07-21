/*	$NetBSD: putchar.c,v 1.21 2010/02/03 15:34:40 roy Exp $	*/

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
static char sccsid[] = "@(#)putchar.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: putchar.c,v 1.21 2010/02/03 15:34:40 roy Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

int
__cputchar(int ch)
{

#ifdef DEBUG
	__CTRACE(__CTRACE_OUTPUT, "__cputchar: %s\n", unctrl(ch));
#endif
	return (putc(ch, _cursesi_screen->outfd));
}

/*
 * This is the same as __cputchar but the extra argument holds the file
 * descriptor to write the output to.  This function can only be used with
 * the "new" libterm interface.
 */
int
__cputchar_args(int ch, void *args)
{
	FILE *outfd = (FILE *) args;

#ifdef DEBUG
	__CTRACE(__CTRACE_OUTPUT, "__cputchar_args: %s on fd %d\n",
	    unctrl(ch), outfd->_file);
#endif
	return putc(ch, outfd);
}

#ifdef HAVE_WCHAR
int
__cputwchar(wchar_t wch)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_OUTPUT, "__cputwchar: 0x%x\n", wch);
#endif
	return (putwc(wch, _cursesi_screen->outfd));
}

/*
 * This is the same as __cputchar but the extra argument holds the file
 * descriptor to write the output to.  This function can only be used with
 * the "new" libterm interface.
 */
int
__cputwchar_args(wchar_t wch, void *args)
{
	FILE *outfd = (FILE *) args;

#ifdef DEBUG
	__CTRACE(__CTRACE_OUTPUT, "__cputwchar_args: 0x%x on fd %d\n",
	    wch, outfd->_file);
#endif
	return putwc(wch, outfd);
}
#endif /* HAVE_WCHAR */
