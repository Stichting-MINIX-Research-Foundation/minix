/*	$NetBSD$	*/

/*-
 * Copyright (c) 2011
 *      The NetBSD Foundation, Inc. All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann.
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

/*
 * Copyright (c) 1993
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

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
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

/*
 * Derived from lib/libsa/ls.c: replaces its ls machinery so the
 * sorted, pattern-filtered directory scan is also available through
 * dir_foreach() for glob expansion (load_mods).  The filesystems'
 * *_ls operations call lsadd/lsprint/lsfree from here.
 */

#include "efiboot.h"

#include "stand.h"
#include "ls.h"
#include <sys/stat.h>
#include <lib/libkern/libkern.h>

static void (*ls_callback)(const char *, const char *);

void
ls(const char *path)
{
	int             fd;
	struct stat     sb;
	size_t          size;
	const char	*fname = 0;
	char		*p;
	struct open_file *f;

	if ((fd = open(path, 0)) < 0
	    || fstat(fd, &sb) < 0
	    || (sb.st_mode & S_IFMT) != S_IFDIR) {
		/* Path supplied isn't a directory, open parent
		   directory and list matching files. */
		if (fd >= 0)
			close(fd);
		fname = strrchr(path, '/');
		if (fname) {
			size = fname - path;
			fname++;
			p = alloc(size + 1);
			if (!p)
				goto out;
			memcpy(p, path, size);
			p[size] = 0;
			fd = open(p, 0);
			dealloc(p, size + 1);
		} else {
			fd = open("", 0);
			fname = path;
		}

		if (fd < 0) {
			printf("ls: %s\n", strerror(errno));
			return;
		}
		if (fstat(fd, &sb) < 0) {
			printf("stat: %s\n", strerror(errno));
			goto out;
		}
		if ((sb.st_mode & S_IFMT) != S_IFDIR) {
			printf("%s: %s\n", path, strerror(ENOTDIR));
			goto out;
		}
	}

	f = &files[fd];

#if !defined(LIBSA_NO_FD_CHECKING)
	if ((unsigned int)fd >= SOPEN_MAX || f->f_flags == 0) {
		errno = EBADF;
		goto out;
	}
#endif

#if !defined(LIBSA_NO_RAW_ACCESS)
	/* operation not defined on raw devices */
	if (f->f_flags & F_RAW) {
		errno = EOPNOTSUPP;
		goto out;
	}
#endif

	if (FS_LS(f->f_ops) != NULL)
		FS_LS(f->f_ops)(f, fname);
	else
		printf("no ls support for this file system\n");

out:
	close(fd);
}

/*
 * Enumerate the entries matching path (a directory, or a directory
 * plus fnmatch pattern as its last component) in name order.
 */
void
dir_foreach(const char *path, void (*fn)(const char *, const char *))
{
	ls_callback = fn;
	ls(path);
	ls_callback = NULL;
}

struct lsentry {
	struct lsentry *e_next;
	uint32_t e_ino;
	const char *e_type;
	char	e_name[1];
};

__compactcall void
lsadd(lsentry_t **names, const char *pattern, const char *name, size_t namelen,
    uint32_t ino, const char *type)
{
	lsentry_t *n, **np;

	if (pattern && !fnmatch(name, pattern))
		return;

	n = alloc(sizeof *n + namelen);
	if (!n) {
		printf("%d: %.*s (%s)\n", ino, (int)namelen, name, type);
		return;
	}

	n->e_ino = ino;
	n->e_type = type;
	memcpy(n->e_name, name, namelen);
	n->e_name[namelen] = '\0';

	for (np = names; *np; np = &(*np)->e_next) {
		if (strcmp(n->e_name, (*np)->e_name) < 0)
			break;
	}
	n->e_next = *np;
	*np = n;
}

__compactcall void
lsprint(lsentry_t *names) {
	if (ls_callback != NULL) {
		while (names) {
			ls_callback(names->e_name, names->e_type);
			names = names->e_next;
		}
		return;
	}
	if (!names) {
		printf("not found\n");
		return;
	}
	do {
		lsentry_t *n = names;
		printf("%d: %s (%s)\n", n->e_ino, n->e_name, n->e_type);
		names = n->e_next;
	} while (names);
}

__compactcall void
lsfree(lsentry_t *names) {
	if (!names)
		return;
	do {
		lsentry_t *n = names;
		names = n->e_next;
		dealloc(n, 0);
	} while (names);
}

__compactcall void
lsunsup(const char *name) {
	printf("The ls command is not currently supported for %s\n", name);
}
