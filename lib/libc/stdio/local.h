/*	$NetBSD: local.h,v 1.38 2014/06/18 17:47:58 christos Exp $	*/

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
 *
 *	@(#)local.h	8.3 (Berkeley) 7/3/94
 */

#include "wcio.h"
#include "fileext.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>

/*
 * Information local to this implementation of stdio,
 * in particular, macros and private variables.
 */

extern int	__sflush(FILE *);
extern FILE	*__sfp(void);
extern void	__sfpinit(FILE *);
extern int	__srefill(FILE *);
extern ssize_t	__sread(void *, void *, size_t);
extern ssize_t	__swrite(void *, const void *, size_t);
extern off_t	__sseek(void *, off_t, int);
extern int	__sclose(void *);
extern void	__sinit(void);
extern void	_cleanup(void);
extern void	(*__cleanup)(void);
extern void	__smakebuf(FILE *);
extern int	__swhatbuf(FILE *, size_t *, int *);
extern int	_fwalk(int (*)(FILE *));
extern char    *_mktemp(char *);
extern int	__swsetup(FILE *);
extern int	__sflags(const char *, int *);
extern int	__svfscanf(FILE * __restrict, const char * __restrict,
    va_list) __scanflike(2, 0);
extern int	__svfscanf_l(FILE * __restrict, locale_t,
    const char * __restrict, va_list) __scanflike(3, 0);
extern int	__svfscanf_unlocked_l(FILE * __restrict, locale_t,
    const char * __restrict, va_list) __scanflike(3, 0);
extern int	__vfprintf_unlocked_l(FILE * __restrict, locale_t,
    const char * __restrict, va_list) __printflike(3, 0);


extern int	__sdidinit;

extern wint_t	__fgetwc_unlock(FILE *);
extern wint_t	__fputwc_unlock(wchar_t, FILE *);

extern ssize_t	__getdelim(char **__restrict, size_t *__restrict, int,
    FILE *__restrict);
extern char	*__fgetstr(FILE * __restrict, size_t * __restrict, int);
extern int 	 __vfwprintf_unlocked_l(FILE *, locale_t, const wchar_t *, va_list);
extern int	 __vfwscanf_unlocked_l(FILE * __restrict, locale_t,
    const wchar_t * __restrict, va_list);

/*
 * Return true iff the given FILE cannot be written now.
 */
#define	cantwrite(fp) \
	((((fp)->_flags & __SWR) == 0 || (fp)->_bf._base == NULL) && \
	 __swsetup(fp))

/*
 * Test whether the given stdio file has an active ungetc buffer;
 * release such a buffer, without restoring ordinary unread data.
 */
#define	HASUB(fp) (_UB(fp)._base != NULL)
#define	FREEUB(fp) { \
	if (_UB(fp)._base != (fp)->_ubuf) \
		free((char *)_UB(fp)._base); \
	_UB(fp)._base = NULL; \
}

/*
 * test for an fgetln() buffer.
 */
#define	FREELB(fp) { \
	free(_EXT(fp)->_fgetstr_buf); \
	_EXT(fp)->_fgetstr_buf = NULL; \
	_EXT(fp)->_fgetstr_len = 0; \
}

extern void __flockfile_internal(FILE *, int);
extern void __funlockfile_internal(FILE *, int);

extern char *__gets(char *);

/*
 * Detect if the current file position fits in a long int.
 */

static __inline bool
__long_overflow(off_t pos)
{
	return (pos < LONG_MIN) || (pos > LONG_MAX);
}
