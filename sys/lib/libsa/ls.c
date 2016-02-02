/* $NetBSD: ls.c,v 1.5 2014/03/20 03:13:18 christos Exp $ */

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


#include "stand.h"
#include "ls.h"
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
#include <sys/param.h>
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#include <sys/stat.h>
#include <lib/libkern/libkern.h>

void
ls(const char *path)
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
{
	load_mods(path, NULL);
}

void
load_mods(const char *path, void (*funcp)(char* arg))
{
#endif /* !defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
	int             fd;
	struct stat     sb;
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
	size_t          size = -1;
#else
	size_t          size;
#endif /* !defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */

	const char	*fname = 0;
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
	char		*p = NULL;
#else
	char		*p;
#endif /* !defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
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
#if defined(__minix) && !defined(LIBSA_ENABLE_LOAD_MODS_OP)
			dealloc(p, size + 1);
#endif /* !defined(__minix) && !defined(LIBSA_ENABLE_LOAD_MODS_OP) */
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
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
		FS_LOAD_MODS(f->f_ops)(f, fname, funcp, p);
#else
		FS_LS(f->f_ops)(f, fname);
#endif /* !defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
	else
		printf("no ls support for this file system\n");

out:
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
	/* LSC: MINIX Modification for correct glob support, beware! */
	if (p != NULL)
		dealloc(p, size + 1);
#endif /* !defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
	close(fd);
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

#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
__compactcall void
lsapply(lsentry_t * names, const char * pattern, void (* funcp)(char * arg),
	char * path)
{
	if (!names) {
		printf("not found\n");
		return;
	}
	if (NULL == funcp) {
		printf("no callback provided\n");
		return;
	}
	do {
		lsentry_t *n = names;
		char namebuf[MAXPATHLEN+1];
		namebuf[0] = '\0';

		if (path != pattern) {
			strcpy(namebuf, path);
			namebuf[strlen(path)] = '/';
			namebuf[strlen(path) + 1] = '\0';
		}
		strcat(namebuf, n->e_name);

		funcp(namebuf);

		names = n->e_next;
	} while (names);
}
#endif /* !defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#if defined(__minix)
__compactcall void
load_modsunsup(const char *name) {
	printf("The load_mods command is not currently supported for %s\n", name);
}
#endif /* defined(__minix) */

__compactcall void
lsprint(lsentry_t *names) {
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
