/*	$NetBSD: compat_stat.c,v 1.2 2009/01/11 02:46:27 christos Exp $	*/

/*
 * Copyright (c) 1997 Frank van der Linden
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_stat.c,v 1.2 2009/01/11 02:46:27 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include <sys/types.h>
#include <sys/stat.h>
#include <compat/sys/time.h>
#include <compat/sys/stat.h>

__warn_references(stat,
    "warning: reference to compatibility stat(); include <sys/stat.h> to generate correct reference")

__warn_references(fstat,
    "warning: reference to compatibility fstat(); include <sys/stat.h> to generate correct reference")

__warn_references(lstat,
    "warning: reference to compatibility lstat(); include <sys/stat.h> to generate correct reference")

/*
 * Convert from a new to an old stat structure.
 */

static void cvtstat(const struct stat *, struct stat12 *);

static void
cvtstat(const struct stat *st, struct stat12 *ost)
{

	ost->st_dev = (uint32_t)st->st_dev;
	ost->st_ino = (uint32_t)st->st_ino;
	ost->st_mode = st->st_mode;
	if (st->st_nlink >= (1 << 15))
		ost->st_nlink = (1 << 15) - 1;
	else
		ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = (uint32_t)st->st_rdev;
	ost->st_atimespec.tv_sec = (int32_t)st->st_atimespec.tv_sec;
	ost->st_atimespec.tv_nsec = st->st_atimespec.tv_nsec;
	ost->st_mtimespec.tv_sec = (int32_t)st->st_mtimespec.tv_sec;
	ost->st_mtimespec.tv_nsec = st->st_mtimespec.tv_nsec;
	ost->st_ctimespec.tv_sec = (int32_t)st->st_ctimespec.tv_sec;
	ost->st_ctimespec.tv_nsec = st->st_ctimespec.tv_nsec;
	ost->st_size = st->st_size;
	ost->st_blocks = st->st_blocks;
	ost->st_blksize = st->st_blksize;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

int
stat(const char *file, struct stat12 *ost)
{
	struct stat nst;
	int ret;

	if ((ret = __stat50(file, &nst)) < 0)
		return ret;
	cvtstat(&nst, ost);
	return ret;
}

int
fstat(int f, struct stat12 *ost)
{
	struct stat nst;
	int ret;

	if ((ret = __fstat50(f, &nst)) < 0)
		return ret;
	cvtstat(&nst, ost);
	return ret;
}

int
lstat(const char *file, struct stat12 *ost)
{
	struct stat nst;
	int ret;

	if ((ret = __lstat50(file, &nst)) < 0)
		return ret;
	cvtstat(&nst, ost);
	return ret;
}
