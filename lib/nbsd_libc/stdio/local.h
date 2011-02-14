/*	$NetBSD: local.h,v 1.29 2010/10/24 17:44:32 tron Exp $	*/

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
#include <stdbool.h>

/*
 * Information local to this implementation of stdio,
 * in particular, macros and private variables.
 */

extern int	__sflush __P((FILE *));
extern FILE	*__sfp __P((void));
extern void	__sfpinit __P((FILE *));
extern int	__srefill __P((FILE *));
extern int	__sread __P((void *, char *, int));
extern int	__swrite __P((void *, char const *, int));
extern fpos_t	__sseek __P((void *, fpos_t, int));
extern int	__sclose __P((void *));
extern void	__sinit __P((void));
extern void	_cleanup __P((void));
extern void	(*__cleanup) __P((void));
extern void	__smakebuf __P((FILE *));
extern int	__swhatbuf __P((FILE *, size_t *, int *));
extern int	_fwalk __P((int (*)(FILE *)));
extern char	*_mktemp __P((char *));
extern int	__swsetup __P((FILE *));
extern int	__sflags __P((const char *, int *));
extern int	__svfscanf __P((FILE * __restrict, const char * __restrict,
		    _BSD_VA_LIST_))
		    __attribute__((__format__(__scanf__, 2, 0)));
extern int	__svfscanf_unlocked __P((FILE * __restrict, const char * __restrict,
		    _BSD_VA_LIST_))
		    __attribute__((__format__(__scanf__, 2, 0)));
extern int	__vfprintf_unlocked __P((FILE * __restrict, const char * __restrict,
		    _BSD_VA_LIST_));


extern int	__sdidinit;

extern int	__gettemp __P((char *, int *, int));

extern wint_t	__fgetwc_unlock __P((FILE *));
extern wint_t	__fputwc_unlock __P((wchar_t, FILE *));

extern ssize_t	__getdelim(char **__restrict, size_t *__restrict, int,
    FILE *__restrict);
extern char	*__fgetstr __P((FILE * __restrict, size_t * __restrict, int));
extern int 	 __vfwprintf_unlocked __P((FILE *, const wchar_t *,
    _BSD_VA_LIST_));
extern int	 __vfwscanf_unlocked __P((FILE * __restrict,
    const wchar_t * __restrict, _BSD_VA_LIST_));

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

extern void __flockfile_internal __P((FILE *, int));
extern void __funlockfile_internal __P((FILE *, int));

/*
 * Detect if the current file position fits in a long int.
 */

static __inline bool
__fpos_overflow(fpos_t pos)
{
  return (pos < LONG_MIN) || (pos > LONG_MAX);
}
