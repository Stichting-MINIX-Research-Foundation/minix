/*	$NetBSD: scandir.c,v 1.27 2012/03/13 21:13:36 christos Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)scandir.c	8.3 (Berkeley) 1/2/94";
#else
__RCSID("$NetBSD: scandir.c,v 1.27 2012/03/13 21:13:36 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * Scan the directory dirname calling selectfn to make a list of selected
 * directory entries then sort using qsort and compare routine dcomp.
 * Returns the number of entries and a pointer to a list of pointers to
 * struct dirent (through namelist). Returns -1 if there were any errors.
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/*
 * Compute an estimate of the number of entries in a directory based on
 * the file size. Returns the estimated number of entries or 0 on failure.
 */
static size_t
dirsize(int fd, size_t olen)
{
	struct stat stb;
	size_t nlen;

	if (fstat(fd, &stb) == -1)
		return 0;
	/*
	 * Estimate the array size by taking the size of the directory file
	 * and dividing it by a multiple of the minimum size entry. 
	 */
	nlen = (size_t)(stb.st_size / _DIRENT_MINSIZE((struct dirent *)0));
	/*
	 * If the size turns up 0, switch to an alternate strategy and use the
	 * file size as the number of entries like ZFS returns. If that turns
	 * out to be 0 too return a minimum of 10 entries, plus the old length.
	 */
	if (nlen == 0)
		nlen = (size_t)(stb.st_size ? stb.st_size : 10);
	return olen + nlen;
}

int
scandir(const char *dirname, struct dirent ***namelist,
    int (*selectfn)(const struct dirent *),
    int (*dcomp)(const void *, const void *))
{
	struct dirent *d, *p, **names, **newnames;
	size_t nitems, arraysz;
	DIR *dirp;

	_DIAGASSERT(dirname != NULL);
	_DIAGASSERT(namelist != NULL);

	if ((dirp = opendir(dirname)) == NULL)
		return -1;

	if ((arraysz = dirsize(dirp->dd_fd, 0)) == 0)
		goto bad;

	names = malloc(arraysz * sizeof(*names));
	if (names == NULL)
		goto bad;

	nitems = 0;
	while ((d = readdir(dirp)) != NULL) {
		if (selectfn != NULL && !(*selectfn)(d))
			continue;	/* just selected names */

		/*
		 * Check to make sure the array has space left and
		 * realloc the maximum size.
		 */
		if (nitems >= arraysz) {
			if ((arraysz = dirsize(dirp->dd_fd, arraysz)) == 0)
				goto bad2;
			newnames = realloc(names, arraysz * sizeof(*names));
			if (newnames == NULL)
				goto bad2;
			names = newnames;
		}

		/*
		 * Make a minimum size copy of the data
		 */
		p = malloc((size_t)_DIRENT_SIZE(d));
		if (p == NULL)
			goto bad2;
		p->d_fileno = d->d_fileno;
		p->d_reclen = d->d_reclen;
#ifndef __minix
		p->d_type = d->d_type;
		p->d_namlen = d->d_namlen;
		(void)memmove(p->d_name, d->d_name, (size_t)(p->d_namlen + 1));
#else
		(void)memmove(p->d_name, d->d_name, (size_t)(strlen(d->d_name) + 1));
#endif
		names[nitems++] = p;
	}
	(void)closedir(dirp);
	if (nitems && dcomp != NULL)
		qsort(names, nitems, sizeof(*names), dcomp);
	*namelist = names;
	_DIAGASSERT(__type_fit(int, nitems));
	return (int)nitems;

bad2:
	while (nitems-- > 0)
		free(names[nitems]);
	free(names);
bad:
	(void)closedir(dirp);
	return -1;
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(scandir, __scandir30)
#endif
