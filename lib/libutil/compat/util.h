/*	$NetBSD: util.h,v 1.2 2009/01/11 02:57:18 christos Exp $	*/

/*-
 * Copyright (c) 1995
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

#ifndef _COMPAT_UTIL_H_
#define	_COMPAT_UTIL_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <compat/include/pwd.h>
#include <compat/include/utmp.h>
#include <compat/include/utmpx.h>
#include <machine/ansi.h>

void		login(const struct utmp50 *);
void		loginx(const struct utmpx50 *);

int32_t		parsedate(const char *, const int32_t *, const int *);

void		pw_copy(int, int, struct passwd50 *, struct passwd50 *);
int		pw_copyx(int, int, struct passwd50 *, struct passwd50 *,
    char *, size_t);
void		pw_getpwconf(char *, size_t, const struct passwd50 *,
    const char *);

void		__login50(const struct utmp *);
void		__loginx50(const struct utmpx *);

time_t		__parsedate50(const char *, const time_t *, const int *);

void		__pw_copy50(int, int, struct passwd *, struct passwd *);
int		__pw_copyx50(int, int, struct passwd *, struct passwd *,
    char *, size_t);
void		__pw_getpwconf50(char *, size_t, const struct passwd *,
    const char *);
#endif /* !_COMPAT_UTIL_H_ */
