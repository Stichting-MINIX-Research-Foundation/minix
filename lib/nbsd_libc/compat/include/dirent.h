/*	$NetBSD: dirent.h,v 1.7 2009/02/24 18:42:36 christos Exp $	*/

/*-
 * Copyright (c) 1989, 1993
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
 *
 *	@(#)dirent.h	8.2 (Berkeley) 7/28/94
 */

#ifndef _COMPAT_DIRENT_H_
#define _COMPAT_DIRENT_H_

#include <compat/sys/dirent.h>

__BEGIN_DECLS

DIR *opendir(const char *);
DIR *__opendir30(const char *);

int readdir_r(DIR *, struct dirent12 * __restrict,
    struct dirent12 ** __restrict);
int __readdir_r30(DIR * __restrict, struct dirent * __restrict,
    struct dirent ** __restrict);

struct dirent12 *readdir(DIR *);
struct dirent *__readdir30(DIR *);

#if defined(_NETBSD_SOURCE)

struct dirent12 *_readdir_unlocked(DIR *, int);
struct dirent *___readdir_unlocked50(DIR *, int);
struct dirent *___readdir_unlocked30(DIR *);

DIR *__opendir2(const char *, int);
DIR *__opendir230(const char *, int);

int scandir(const char *, struct dirent12 ***,
    int (*)(const struct dirent12 *), int (*)(const void *, const void *));
int __scandir30(const char *, struct dirent ***,
    int (*)(const struct dirent *), int (*)(const void *, const void *));

int getdents(int, char *, size_t);
int __getdents30(int, char *, size_t);

int alphasort(const void *, const void *);
int __alphasort30(const void *, const void *);

int getdirentries(int, char *, int, long *);

#endif /* defined(_NETBSD_SOURCE) */

__END_DECLS

#endif /* !_COMPAT_DIRENT_H_ */
