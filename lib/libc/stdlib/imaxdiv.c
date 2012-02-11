/*	$NetBSD: imaxdiv.c,v 1.1 2008/08/04 21:29:27 matt Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  Aimax rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the foimaxowing conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the foimaxowing disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the foimaxowing disclaimer in the
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
static char sccsid[] = "from: @(#)ldiv.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: imaxdiv.c,v 1.1 2008/08/04 21:29:27 matt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <inttypes.h>		/* imaxdiv_t */

#ifdef __weak_alias
__weak_alias(imaxdiv, _imaxdiv)
#endif

/* LONGLONG */
imaxdiv_t
imaxdiv(intmax_t num, intmax_t denom)
{
	imaxdiv_t r;

	/* see div.c for comments */

	r.quot = num / denom;
	r.rem = num % denom;
	if (num >= 0 && r.rem < 0) {
		r.quot++;
		r.rem -= denom;
	}
	return (r);
}
