/* $NetBSD: misc.c,v 1.20 2013/07/16 17:47:43 christos Exp $ */

/*-
 * Copyright (c) 1980, 1991, 1993
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
static char sccsid[] = "@(#)misc.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: misc.c,v 1.20 2013/07/16 17:47:43 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "csh.h"
#include "extern.h"

static int renum(int, int);

int
any(const char *s, int c)
{
    if (!s)
	return (0);		/* Check for nil pointer */
    while (*s)
	if (*s++ == c)
	    return (1);
    return (0);
}

char *
strsave(const char *s)
{
    const char *n;
    char *p, *r;

    if (s == NULL)
	s = "";
    for (n = s; *n++;)
	continue;
    r = p = xmalloc((size_t)(n - s) * sizeof(*p));
    while ((*p++ = *s++) != '\0')
	continue;
    return (r);
}

Char **
blkend(Char **up)
{
    while (*up)
	up++;
    return (up);
}


void
blkpr(FILE *fp, Char **av)
{
    for (; *av; av++) {
	(void)fprintf(fp, "%s", vis_str(*av));
	if (av[1])
	    (void)fprintf(fp, " ");
    }
}

int
blklen(Char **av)
{
    int i;

    i = 0;
    while (*av++)
	i++;
    return (i);
}

Char **
blkcpy(Char **oav, Char **bv)
{
    Char **av;

    av = oav;
    while ((*av++ = *bv++) != NULL)
	continue;
    return (oav);
}

Char **
blkcat(Char **up, Char **vp)
{
    (void)blkcpy(blkend(up), vp);
    return (up);
}

void
blkfree(Char **av0)
{
    Char **av;

    av = av0;
    if (!av0)
	return;
    for (; *av; av++)
	xfree((ptr_t) * av);
    xfree((ptr_t) av0);
}

Char **
saveblk(Char **v)
{
    Char **newv, **onewv;

    if (v == NULL)
	return NULL;

    newv = xcalloc((size_t)(blklen(v) + 1), sizeof(*newv));
    onewv = newv;
    while (*v)
	*newv++ = Strsave(*v++);
    return (onewv);
}

#ifdef NOTUSED
char *
strstr(char *s, char *t)
{
    do {
	char *ss;
	char *tt;

	ss = s;
	tt = t;

	do
	    if (*tt == '\0')
		return (s);
	while (*ss++ == *tt++);
    } while (*s++ != '\0');
    return (NULL);
}

#endif /* NOTUSED */

#ifndef SHORT_STRINGS
char *
strspl(char *cp, char *dp)
{
    char *ep, *p, *q;

    if (!cp)
	cp = "";
    if (!dp)
	dp = "";
    for (p = cp; *p++;)
	continue;
    for (q = dp; *q++;)
	continue;
    ep = xmalloc((size_t)(((p - cp) + (q - dp) - 1) * sizeof(*ep)));
    for (p = ep, q = cp; *p++ = *q++;)
	continue;
    for (p--, q = dp; *p++ = *q++;)
	continue;
    return (ep);
}

#endif

Char **
blkspl(Char **up, Char **vp)
{
    Char **wp;

    wp = xcalloc((size_t)(blklen(up) + blklen(vp) + 1), sizeof(*wp));
    (void)blkcpy(wp, up);
    return (blkcat(wp, vp));
}

Char
lastchr(Char *cp)
{
    if (!cp)
	return (0);
    if (!*cp)
	return (0);
    while (cp[1])
	cp++;
    return (*cp);
}

/*
 * This routine is called after an error to close up
 * any units which may have been left open accidentally.
 */
void
closem(void)
{
    int f;
    int nofile;

#ifdef F_CLOSEM
    nofile = FOLDSTD + 1;
    if (fcntl(nofile, F_CLOSEM, 0) == -1)
#endif
	nofile = NOFILE;

    for (f = 0; f < nofile; f++)
	if (f != SHIN && f != SHOUT && f != SHERR && f != OLDSTD &&
	    f != FSHTTY)
	    (void) close(f);
}

void
donefds(void)
{
    (void)close(0);
    (void)close(1);
    (void)close(2);

    didfds = 0;
}

/*
 * Move descriptor i to j.
 * If j is -1 then we just want to get i to a safe place,
 * i.e. to a unit > 2.  This also happens in dcopy.
 */
int
dmove(int i, int j)
{
    if (i == j || i < 0)
	return (i);
    if (j >= 0) {
	(void)dup2(i, j);
	if (j != i)
	    (void)close(i);
	return (j);
    }
    j = dcopy(i, j);
    if (j != i)
	(void)close(i);
    return (j);
}

int
dcopy(int i, int j)
{
    if (i == j || i < 0 || (j < 0 && i > 2))
	return (i);
    if (j >= 0) {
	(void)dup2(i, j);
	return (j);
    }
    return (renum(i, j));
}

static int
renum(int i, int j)
{
    int k;

    k = dup(i);
    if (k < 0)
	return (-1);
    if (j == -1 && k > 2)
	return (k);
    if (k != j) {
	j = renum(k, j);
	(void)close(k);
	return (j);
    }
    return (k);
}

/*
 * Left shift a command argument list, discarding
 * the first c arguments.  Used in "shift" commands
 * as well as by commands like "repeat".
 */
void
lshift(Char **v, size_t c)
{
    Char **u;

    for (u = v; *u && c-- > 0; u++)
	xfree((ptr_t) *u);
    (void)blkcpy(v, u);
}

int
number(Char *cp)
{
    if (!cp)
	return(0);
    if (*cp == '-') {
	cp++;
	if (!Isdigit(*cp))
	    return (0);
	cp++;
    }
    while (*cp && Isdigit(*cp))
	cp++;
    return (*cp == 0);
}

Char **
copyblk(Char **v)
{
    Char **nv;

    nv = xcalloc((size_t)(blklen(v) + 1), sizeof(*nv));

    return (blkcpy(nv, v));
}

#ifndef SHORT_STRINGS
char *
strend(char *cp)
{
    if (!cp)
	return (cp);
    while (*cp)
	cp++;
    return (cp);
}

#endif /* SHORT_STRINGS */

Char *
strip(Char *cp)
{
    Char *dp;

    dp = cp;
    if (!cp)
	return (cp);
    while ((*dp++ &= TRIM) != '\0')
	continue;
    return (cp);
}

Char *
quote(Char *cp)
{
    Char *dp;

    dp = cp;
    if (!cp)
	return (cp);
    while (*dp != '\0')
	*dp++ |= QUOTE;
    return (cp);
}

void
udvar(Char *name)
{
    setname(vis_str(name));
    stderror(ERR_NAME | ERR_UNDVAR);
    /* NOTREACHED */
}

int
prefix(Char *sub, Char *str)
{
    for (;;) {
	if (*sub == 0)
	    return (1);
	if (*str == 0)
	    return (0);
	if (*sub++ != *str++)
	    return (0);
    }
}
