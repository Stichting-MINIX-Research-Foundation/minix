/*	$NetBSD: bcmp.c,v 1.7 2012/03/09 15:41:16 christos Exp $	*/

/*
 * Copyright (c) 1987, 1993
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)bcmp.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: bcmp.c,v 1.7 2012/03/09 15:41:16 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */


#if defined(_KERNEL) || defined(_STANDALONE)
#include <lib/libkern/libkern.h>
#if defined(_STANDALONE)
#include <lib/libsa/stand.h>
#endif
#else
#include <assert.h>
#include <string.h>
#endif

/*
 * bcmp -- vax cmpc3 instruction
 */
int
bcmp(const void *b1, const void *b2, size_t length)
{
	const char *p1 = b1, *p2 = b2;

	_DIAGASSERT(b1 != 0);
	_DIAGASSERT(b2 != 0);

	if (length == 0)
		return(0);
	do
		if (*p1++ != *p2++)
			break;
	while (--length);
	return length != 0;
}
