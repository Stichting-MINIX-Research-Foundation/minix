/* $NetBSD: str.c,v 1.15 2013/07/16 17:47:43 christos Exp $ */

/*-
 * Copyright (c) 1991, 1993
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
static char sccsid[] = "@(#)str.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: str.c,v 1.15 2013/07/16 17:47:43 christos Exp $");
#endif
#endif /* not lint */

#define MALLOC_INCR 128

/*
 * tc.str.c: Short string package
 *	     This has been a lesson of how to write buggy code!
 */

#include <sys/types.h>

#include <stdarg.h>
#include <vis.h>

#include "csh.h"
#include "extern.h"

#ifdef SHORT_STRINGS

Char **
blk2short(char **src)
{
    Char **dst, **sdst;
    size_t n;

    /*
     * Count
     */
    for (n = 0; src[n] != NULL; n++)
	continue;
    sdst = dst = xmalloc((size_t)((n + 1) * sizeof(*dst)));

    for (; *src != NULL; src++)
	*dst++ = SAVE(*src);
    *dst = NULL;
    return (sdst);
}

char **
short2blk(Char *const *src)
{
    char **dst, **sdst;
    size_t n;

    /*
     * Count
     */
    for (n = 0; src[n] != NULL; n++)
	continue;
    sdst = dst = xmalloc((size_t)((n + 1) * sizeof(*dst)));

    for (; *src != NULL; src++)
	*dst++ = strsave(short2str(*src));
    *dst = NULL;
    return (sdst);
}

Char *
str2short(const char *src)
{
    static Char *sdst;
    Char *dst, *edst;
    static size_t dstsize = 0;

    if (src == NULL)
	return (NULL);

    if (sdst == (NULL)) {
	dstsize = MALLOC_INCR;
	sdst = xmalloc((size_t)dstsize * sizeof(*sdst));
    }

    dst = sdst;
    edst = &dst[dstsize];
    while (*src) {
	*dst++ = (Char) ((unsigned char) *src++);
	if (dst == edst) {
	    dstsize += MALLOC_INCR;
	    sdst = xrealloc((ptr_t)sdst,
	        (size_t)dstsize * sizeof(*sdst));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}

char *
short2str(const Char *src)
{
    static char *sdst = NULL;
    static size_t dstsize = 0;
    char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == NULL) {
	dstsize = MALLOC_INCR;
	sdst = xmalloc((size_t)dstsize * sizeof(*sdst));
    }
    dst = sdst;
    edst = &dst[dstsize];
    while (*src) {
	*dst++ = (char) *src++;
	if (dst == edst) {
	    dstsize += MALLOC_INCR;
	    sdst = xrealloc((ptr_t)sdst,
	        (size_t)dstsize * sizeof(*sdst));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}

Char *
s_strcpy(Char *dst, const Char *src)
{
    Char *sdst;

    sdst = dst;
    while ((*dst++ = *src++) != '\0')
	continue;
    return (sdst);
}

Char *
s_strncpy(Char *dst, const Char *src, size_t n)
{
    Char *sdst;

    if (n == 0)
	return(dst);

    sdst = dst;
    do
	if ((*dst++ = *src++) == '\0') {
	    while (--n != 0)
		*dst++ = '\0';
	    return(sdst);
	}
    while (--n != 0);
    return (sdst);
}

Char *
s_strcat(Char *dst, const Char *src)
{
    short *sdst;

    sdst = dst;
    while (*dst++)
	continue;
    --dst;
    while ((*dst++ = *src++) != '\0')
	continue;
    return (sdst);
}

#ifdef NOTUSED
Char *
s_strncat(Char *dst, Char *src, size_t n)
{
    Char *sdst;

    if (n == 0)
	return (dst);

    sdst = dst;

    while (*dst++)
	continue;
    --dst;

    do
	if ((*dst++ = *src++) == '\0')
	    return(sdst);
    while (--n != 0)
	continue;

    *dst = '\0';
    return (sdst);
}

#endif

Char *
s_strchr(const Char *str, int ch)
{
    do
	if (*str == ch)
	    return __UNCONST(str);
    while (*str++);
    return (NULL);
}

Char *
s_strrchr(const Char *str, int ch)
{
    const Char *rstr;

    rstr = NULL;
    do
	if (*str == ch)
	    rstr = str;
    while (*str++);
    return __UNCONST(rstr);
}

size_t
s_strlen(const Char *str)
{
    size_t n;

    for (n = 0; *str++; n++)
	continue;
    return (n);
}

int
s_strcmp(const Char *str1, const Char *str2)
{
    for (; *str1 && *str1 == *str2; str1++, str2++)
	continue;
    /*
     * The following case analysis is necessary so that characters which look
     * negative collate low against normal characters but high against the
     * end-of-string NUL.
     */
    if (*str1 == '\0' && *str2 == '\0')
	return (0);
    else if (*str1 == '\0')
	return (-1);
    else if (*str2 == '\0')
	return (1);
    else
	return (*str1 - *str2);
}

int
s_strncmp(const Char *str1, const Char *str2, size_t n)
{
    if (n == 0)
	return (0);
    do {
	if (*str1 != *str2) {
	    /*
	     * The following case analysis is necessary so that characters
	     * which look negative collate low against normal characters
	     * but high against the end-of-string NUL.
	     */
	    if (*str1 == '\0')
		return (-1);
	    else if (*str2 == '\0')
		return (1);
	    else
		return (*str1 - *str2);
	}
        if (*str1 == '\0')
	    return(0);
	str1++, str2++;
    } while (--n != 0);
    return(0);
}

Char *
s_strsave(const Char *s)
{
    const Char *p;
    Char *n;

    if (s == 0)
	s = STRNULL;
    for (p = s; *p++;)
	continue;
    p = n = xmalloc((size_t)(p - s) * sizeof(*n));
    while ((*n++ = *s++) != '\0')
	continue;
    return __UNCONST(p);
}

Char *
s_strspl(const Char *cp, const Char *dp)
{
    Char *ep, *d;
    const Char *p, *q;

    if (!cp)
	cp = STRNULL;
    if (!dp)
	dp = STRNULL;
    for (p = cp; *p++;)
	continue;
    for (q = dp; *q++;)
	continue;
    ep = xmalloc((size_t)((p - cp) + (q - dp) - 1) * sizeof(*ep));
    for (d = ep, q = cp; (*d++ = *q++) != '\0';)
	continue;
    for (d--, q = dp; (*d++ = *q++) != '\0';)
	continue;
    return (ep);
}

Char *
s_strend(const Char *cp)
{
    if (!cp)
	return __UNCONST(cp);
    while (*cp)
	cp++;
    return __UNCONST(cp);
}

Char *
s_strstr(const Char *s, const Char *t)
{
    do {
	const Char *ss = s;
	const Char *tt = t;

	do
	    if (*tt == '\0')
		return __UNCONST(s);
	while (*ss++ == *tt++);
    } while (*s++ != '\0');
    return (NULL);
}
#endif				/* SHORT_STRINGS */

char *
short2qstr(const Char *src)
{
    static char *sdst = NULL;
    static size_t dstsize = 0;
    char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == NULL) {
	dstsize = MALLOC_INCR;
	sdst = xmalloc((size_t)dstsize * sizeof(*sdst));
    }
    dst = sdst;
    edst = &dst[dstsize];
    while (*src) {

	if (*src & QUOTE) {
	    *dst++ = '\\';
	    if (dst == edst) {
		dstsize += MALLOC_INCR;
		sdst = xrealloc((ptr_t) sdst, 
		    (size_t)dstsize * sizeof(*sdst));
		edst = &sdst[dstsize];
		dst = &edst[-MALLOC_INCR];
	    }
	}
	*dst++ = (char) *src++;
	if (dst == edst) {
	    dstsize += MALLOC_INCR;
	    sdst = xrealloc((ptr_t) sdst,
	        (size_t)dstsize * sizeof(*sdst));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}

/*
 * XXX: Should we worry about QUOTE'd chars?
 */
char *
vis_str(const Char *cp)
{
    static char *sdst = NULL;
    static size_t dstsize = 0;
    const Char *dp;
    size_t n;

    if (cp == NULL)
	return (NULL);
    
    for (dp = cp; *dp++;)
	continue;
    n = ((size_t)(dp - cp) << 2) + 1; /* 4 times + NULL */
    if (dstsize < n) {
	sdst = (dstsize ? 
	    xrealloc(sdst, (size_t)n * sizeof(*sdst)) :
	    xmalloc((size_t)n * sizeof(*sdst)));
	dstsize = n;
    }
    /* 
     * XXX: When we are in AsciiOnly we want all characters >= 0200 to
     * be encoded, but currently there is no way in vis to do that.
     */
    (void)strvis(sdst, short2str(cp), VIS_NOSLASH);
    return (sdst);
}
