/*	$NetBSD: longname.c,v 1.16 2004/01/20 08:29:29 wiz Exp $	*/

/*
 * Copyright (c) 1981, 1993
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
static char sccsid[] = "@(#)longname.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: longname.c,v 1.16 2004/01/20 08:29:29 wiz Exp $");
#endif
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * __longname --
 *	Fill in "def" with the long name of the terminal.
 *      This is the original BSD version of longname(), modified to return
 *	at most 128 characters.
 */
char   *
__longname(char *bp, char *def)
{
	char   *cp, *last_bp;
	int	i = 0;

	last_bp = NULL;
	do {
		while (*bp && *bp != ':' && *bp != '|')
			bp++;
		if (*bp == '|') {
			last_bp = bp;
			bp++;
		}
	} while (*bp && *bp != ':');

	if (last_bp != NULL)
		bp = last_bp;

	if (*bp == '|') {
		for (cp = def, ++bp; *bp && *bp != ':' && *bp != '|' &&
		    i < 127;)
			*cp++ = *bp++;
			i++;
		*cp = '\0';
	}
	return (def);
}

/*
 * longname --
 *	Return pointer to the long name of the terminal.
 *	This is the SUS version of longname()
 */
char	*
longname(void)
{
	return (_cursesi_screen->ttytype);
}
