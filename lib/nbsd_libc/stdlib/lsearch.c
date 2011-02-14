/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Roger L. Snyder.
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
static char sccsid[] = "@(#)lsearch.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: lsearch.c,v 1.4 2005/07/16 17:32:09 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <search.h>

typedef int (*cmp_fn_t) __P((const void *, const void *));
static void *linear_base __P((const void *, void *, size_t *, size_t,
			     cmp_fn_t, int));

void *
lsearch(key, base, nelp, width, compar)
	const void *key;
	void *base;
	size_t *nelp, width;
	cmp_fn_t compar;
{

	_DIAGASSERT(key != NULL);
	_DIAGASSERT(base != NULL);
	_DIAGASSERT(compar != NULL);

	return(linear_base(key, base, nelp, width, compar, 1));
}

void *
lfind(key, base, nelp, width, compar)
	const void *key, *base;
	size_t *nelp, width;
	cmp_fn_t compar;
{

	_DIAGASSERT(key != NULL);
	_DIAGASSERT(base != NULL);
	_DIAGASSERT(compar != NULL);

	return(linear_base(key, __UNCONST(base), nelp, width, compar, 0));
}

static void *
linear_base(key, base, nelp, width, compar, add_flag)
	const void *key;
	void *base;
	size_t *nelp, width;
	cmp_fn_t compar;
	int add_flag;
{
	char *element, *end;

	_DIAGASSERT(key != NULL);
	_DIAGASSERT(base != NULL);
	_DIAGASSERT(compar != NULL);

	end = (char *)base + *nelp * width;
	for (element = (char *)base; element < end; element += width)
		if (!compar(element, key))		/* key found */
			return element;

	if (!add_flag)					/* key not found */
		return(NULL);

	/*
	 * The UNIX System User's Manual, 1986 edition claims that
	 * a NULL pointer is returned by lsearch with errno set
	 * appropriately, if there is not enough room in the table
	 * to add a new item.  This can't be done as none of these
	 * routines have any method of determining the size of the
	 * table.  This comment isn't in the 1986-87 System V
	 * manual.
	 */
	++*nelp;
	memcpy(end, key, width);
	return end;
}
