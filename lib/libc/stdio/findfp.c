/*	$NetBSD: findfp.c,v 1.28 2012/03/27 15:05:42 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
static char sccsid[] = "@(#)findfp.c	8.2 (Berkeley) 1/4/94";
#else
__RCSID("$NetBSD: findfp.c,v 1.28 2012/03/27 15:05:42 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "reentrant.h"
#include "local.h"
#include "glue.h"

int	__sdidinit;

#define	NDYNAMIC 10		/* add ten more whenever necessary */

#if !defined(_LIBMINC) && !defined(__kernel__) && defined(__minix)

#define	std(flags, file) { \
	._p = NULL, \
	._r = 0, \
	._w = 0, \
	._flags = (flags), \
	._file = (file),  \
	._bf = { ._base = NULL, ._size = 0 }, \
	._lbfsize = 0,  \
	._cookie = __sF + (file), \
	._close = __sclose, \
	._read = __sread, \
	._seek = __sseek, \
	._write = __swrite, \
	._ext = { ._base = (void *)(__sFext + (file)), ._size = 0 }, \
	._up = NULL, \
        ._ur = 0, \
	._ubuf = { [0] = '\0', [1] = '\0', [2] = '\0' }, \
	._nbuf = { [0] = '\0' }, \
	._flush = NULL, \
	._lb_unused = { '\0' }, \
	._blksize = 0, \
	._offset = (off_t)0, \
}

#else

#define	std(flags, file) { \
	._p = NULL, \
	._r = 0, \
	._w = 0, \
	._flags = (flags), \
	._file = (file),  \
	._bf = { ._base = NULL, ._size = 0 }, \
	._lbfsize = 0,  \
	._cookie = __sF + (file), \
	._close = NULL, \
	._read = NULL, \
	._seek = NULL, \
	._write = NULL, \
	._ext = { ._base = (void *)(__sFext + (file)), ._size = 0 }, \
	._up = NULL, \
        ._ur = 0, \
	._ubuf = { [0] = '\0', [1] = '\0', [2] = '\0' }, \
	._nbuf = { [0] = '\0' }, \
	._flush = NULL, \
	._lb_unused = { '\0' }, \
	._blksize = 0, \
	._offset = (off_t)0, \
}

#endif /* !defined(_LIBMINC) && !defined(__kernel__) */

#if !defined(__kernel__) && defined(__minix)
				/* the usual - (stdin + stdout + stderr) */
static FILE usual[FOPEN_MAX - 3];
static struct __sfileext usualext[FOPEN_MAX - 3];
static struct glue uglue = { 0, FOPEN_MAX - 3, usual };
#endif /* !defined(__kernel__) && defined(__minix) */

#if defined(_REENTRANT) && !defined(__lint__) /* XXX lint is busted */
#define	STDEXT { ._lock = MUTEX_INITIALIZER, ._lockcond = COND_INITIALIZER }
struct __sfileext __sFext[3] = { STDEXT,
				 STDEXT,
				 STDEXT};
#else
struct __sfileext __sFext[3];
#endif

FILE __sF[3] = {
	std(__SRD, STDIN_FILENO),		/* stdin */
	std(__SWR, STDOUT_FILENO),		/* stdout */
	std(__SWR|__SNBF, STDERR_FILENO)	/* stderr */
};

#if !defined(__kernel__) && defined(__minix)
struct glue __sglue = { &uglue, 3, __sF };

void f_prealloc(void);

#ifdef _REENTRANT
rwlock_t __sfp_lock = RWLOCK_INITIALIZER;
#endif

static struct glue *
moreglue(int n)
{
	struct glue *g;
	FILE *p;
	struct __sfileext *pext;
	static FILE empty;

	g = malloc(sizeof(*g) + ALIGNBYTES + n * sizeof(FILE)
	    + n * sizeof(struct __sfileext));
	if (g == NULL)
		return NULL;
	p = (FILE *)ALIGN((u_long)(g + 1));
	g->next = NULL;
	g->niobs = n;
	g->iobs = p;
	pext = (void *)(p + n);
	while (--n >= 0) {
		*p = empty;
		_FILEEXT_SETUP(p, pext);
		p++;
		pext++;
	}
	return g;
}

void
__sfpinit(FILE *fp)
{
	fp->_flags = 1;		/* reserve this slot; caller sets real flags */
	fp->_p = NULL;		/* no current pointer */
	fp->_w = 0;		/* nothing to read or write */
	fp->_r = 0;
	fp->_bf._base = NULL;	/* no buffer */
	fp->_bf._size = 0;
	fp->_lbfsize = 0;	/* not line buffered */
	fp->_file = -1;		/* no file */
/*	fp->_cookie = <any>; */	/* caller sets cookie, _read/_write etc */
	fp->_flush = NULL;	/* default flush */
	_UB(fp)._base = NULL;	/* no ungetc buffer */
	_UB(fp)._size = 0;
	memset(WCIO_GET(fp), 0, sizeof(struct wchar_io_data));
}

/*
 * Find a free FILE for fopen et al.
 */
FILE *
__sfp(void)
{
	FILE *fp;
	int n;
	struct glue *g;

	if (!__sdidinit)
		__sinit();

	rwlock_wrlock(&__sfp_lock);
	for (g = &__sglue;; g = g->next) {
		for (fp = g->iobs, n = g->niobs; --n >= 0; fp++)
			if (fp->_flags == 0)
				goto found;
		if (g->next == NULL && (g->next = moreglue(NDYNAMIC)) == NULL)
			break;
	}
	rwlock_unlock(&__sfp_lock);
	return NULL;
found:
	__sfpinit(fp);
	rwlock_unlock(&__sfp_lock);
	return fp;
}

/*
 * XXX.  Force immediate allocation of internal memory.  Not used by stdio,
 * but documented historically for certain applications.  Bad applications.
 */
void
f_prealloc(void)
{
#if !defined(_LIBMINC) && defined(__minix)
	struct glue *g;
	int n;

	n = (int)sysconf(_SC_OPEN_MAX) - FOPEN_MAX + 20; /* 20 for slop. */
	for (g = &__sglue; (n -= g->niobs) > 0 && g->next; g = g->next)
		continue;
	if (n > 0)
		g->next = moreglue(n);
#endif /* !defined(_LIBMINC) && defined(__minix) */
}

/*
 * exit() calls _cleanup() through *__cleanup, set whenever we
 * open or buffer a file.  This chicanery is done so that programs
 * that do not use stdio need not link it all in.
 *
 * The name `_cleanup' is, alas, fairly well known outside stdio.
 */
void
_cleanup(void)
{
#if !defined(_LIBMINC) && defined(__minix)
	/* (void) _fwalk(fclose); */
	(void) fflush(NULL);			/* `cheating' */
#endif /* !defined(_LIBMINC) && defined(__minix) */
}

/*
 * __sinit() is called whenever stdio's internal variables must be set up.
 */
void
__sinit(void)
{
	int i;

	for (i = 0; i < FOPEN_MAX - 3; i++)
		_FILEEXT_SETUP(&usual[i], &usualext[i]);

	/* make sure we clean up on exit */
	__cleanup = _cleanup;		/* conservative */
	__sdidinit = 1;
}
#endif /* !defined(__kernel__) && defined(__minix) */
