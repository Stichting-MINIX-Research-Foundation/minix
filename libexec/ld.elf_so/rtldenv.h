/*	$NetBSD: rtldenv.h,v 1.11 2011/12/11 11:05:11 joerg Exp $	 */

/*
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTLDENV_H
#define	_RTLDENV_H

#include <stddef.h>
#include <stdarg.h>

void    *xcalloc(size_t);
void    *xmalloc(size_t);
void    *xrealloc(void *, size_t);
char    *xstrdup(const char *);
void	xfree(void *);

#ifdef RTLD_LOADER
void xprintf(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2)));
void xvprintf(const char *, va_list)
    __attribute__((__format__(__printf__, 1, 0)));
void xsnprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(__printf__, 3, 4)));
size_t xvsnprintf(char *, size_t, const char *, va_list)
    __attribute__((__format__(__printf__, 3, 0)));
void xwarn(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2)));
void xwarnx(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2)));
void xerr(int, const char *, ...)
    __attribute__((__noreturn__, __format__(__printf__, 2, 3)));
void xerrx(int, const char *, ...)
    __attribute__((__noreturn__, __format__(__printf__, 2, 3)));

void     xassert(const char *, int, const char *) __dead;
const char *xstrerror(int);
int	xunsetenv(const char *);

# ifdef DEBUG
#  define assert(cond)	((cond) ? (void) 0 : xassert(__FILE__, __LINE__, #cond))
# else
#  define assert(cond)	(void) 0
# endif
#else
# include <assert.h>
# include <stdio.h>
# include <err.h>
 
# define xprintf	printf
# define xvprintf	vprintf
# define xsnprintf	snprintf
# define xvsnprintf	vsnprintf
# define xwarn		warn
# define xwarnx		warnx
# define xerr		err
# define xerrx		errx
# define xassert	assert
# define xstrerror	strerror
#endif

#endif /* _RTLDENV_H */
