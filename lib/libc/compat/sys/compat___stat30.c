/*	$NetBSD: compat___stat30.c,v 1.4 2013/10/04 21:07:37 christos Exp $	*/

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
__RCSID("$NetBSD: compat___stat30.c,v 1.4 2013/10/04 21:07:37 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <compat/sys/time.h>
#include <compat/sys/stat.h>
#include <compat/sys/mount.h>

__warn_references(__stat30,
    "warning: reference to compatibility __stat30(); include <sys/stat.h> to generate correct reference")

__warn_references(__fstat30,
    "warning: reference to compatibility __fstat30(); include <sys/stat.h> to generate correct reference")

__warn_references(__lstat30,
    "warning: reference to compatibility __lstat30(); include <sys/stat.h> to generate correct reference")

__warn_references(__fhstat40,
    "warning: reference to compatibility __fhstat40(); include <sys/mount.h> to generate correct reference")


__strong_alias(__stat30, __compat___stat30)
__strong_alias(__fstat30, __compat___fstat30)
__strong_alias(__lstat30, __compat___lstat30)
__strong_alias(__fhstat40, __compat___fhstat40)

/*
 * Convert from a new to an old stat structure.
 */

static void cvtstat(struct stat30 *, const struct stat *);

static void
cvtstat(struct stat30 *ost, const struct stat *st)
{

	ost->st_dev = (uint32_t)st->st_dev;
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode;
	ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = (uint32_t)st->st_rdev;
	timespec_to_timespec50(&st->st_atimespec, &ost->st_atimespec);
	timespec_to_timespec50(&st->st_mtimespec, &ost->st_mtimespec);
	timespec_to_timespec50(&st->st_ctimespec, &ost->st_ctimespec);
	timespec_to_timespec50(&st->st_birthtimespec, &ost->st_birthtimespec);
	ost->st_size = st->st_size;
	ost->st_blocks = st->st_blocks;
	ost->st_blksize = st->st_blksize;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

int
__compat___stat30(const char *file, struct stat30 *ost)
{
	struct stat nst;
	int ret;

	if ((ret = __stat50(file, &nst)) == -1)
		return ret;
	cvtstat(ost, &nst);
	return ret;
}

int
__compat___fstat30(int f, struct stat30 *ost)
{
	struct stat nst;
	int ret;

	if ((ret = __fstat50(f, &nst)) == -1)
		return ret;
	cvtstat(ost, &nst);
	return ret;
}

int
__compat___lstat30(const char *file, struct stat30 *ost)
{
	struct stat nst;
	int ret;

	if ((ret = __lstat50(file, &nst)) == -1)
		return ret;
	cvtstat(ost, &nst);
	return ret;
}

int
__compat___fhstat40(const void *fh, size_t fh_size, struct stat30 *ost)
{
	struct stat nst;
	int ret;

	if ((ret = __fhstat50(fh, fh_size, &nst)) == -1)
		return ret;
	cvtstat(ost, &nst);
	return ret;
}
