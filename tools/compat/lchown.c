/*	$NetBSD: lchown.c,v 1.4 2008/04/28 20:24:12 martin Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Emulate lchown(2), checking path with lstat(2) first to ensure that
 * it's not a symlink, and then call chown(2) */

#if !defined(__minix) && !defined(_LIBC)
#include "nbtool_config.h"
#endif /* !defined(__minix) && !defined(_LIBC) */

#if !HAVE_LCHOWN
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#if defined(__minix) && defined(__weak_alias) && defined(_LIBC)
__weak_alias(__posix_lchown, lchown)
#endif /* defined(__minix) && defined(__weak_alias) && defined(_LIBC) */

int
lchown(const char *path, uid_t owner, gid_t group)
{
	struct stat psb;

	if (lstat(path, &psb) == -1)
		return -1;
	if (S_ISLNK(psb.st_mode)) {
		return 0;
	}
	return (chown(path, owner, group));
}
#endif
