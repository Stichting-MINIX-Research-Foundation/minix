/*	$NetBSD: initdir.c,v 1.3 2012/03/13 21:13:36 christos Exp $	*/

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
__RCSID("$NetBSD: initdir.c,v 1.3 2012/03/13 21:13:36 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#ifdef __minix
/* NetBSD BUG on !_REENTRANT */
#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <sys/types.h>

#endif

#include "reentrant.h"
#include "extern.h"

#include <sys/param.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dirent_private.h"

#define	MAXITERATIONS	100

int
_initdir(DIR *dirp, int fd, const char *name)
{
	int flags = dirp->dd_flags;
	int pagesz;
	int incr;

	/*
	 * If the machine's page size is an exact multiple of DIRBLKSIZ,
	 * use a buffer that is cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdents()
	 */
	if (((pagesz = getpagesize()) % DIRBLKSIZ) == 0)
		incr = pagesz;
	else
		incr = DIRBLKSIZ;

	if ((flags & DTF_REWIND) && name == NULL) {
		return EINVAL;
	}
	if ((flags & __DTF_READALL) != 0) {
		size_t len;
		size_t space;
		char *buf, *nbuf;
		char *ddptr;
		char *ddeptr;
		int n;
		struct dirent **dpv;
		int i;

		/*
		 * The strategy here for directories on top of a union stack
		 * is to read all the directory entries into a buffer, sort
		 * the buffer, and remove duplicate entries by setting the
		 * inode number to zero.
		 *
		 * For directories on an NFS mounted filesystem, we try
	 	 * to get a consistent snapshot by trying until we have
		 * successfully read all of the directory without errors
		 * (i.e. 'bad cookie' errors from the server because
		 * the directory was modified). These errors should not
		 * happen often, but need to be dealt with.
		 */
		i = 0;
retry:
		len = 0;
		space = 0;
		buf = 0;
		ddptr = 0;

		do {
			/*
			 * Always make at least DIRBLKSIZ bytes
			 * available to getdents
			 */
			if (space < DIRBLKSIZ) {
				space += incr;
				len += incr;
				nbuf = realloc(buf, len);
				if (nbuf == NULL) {
					dirp->dd_buf = buf;
					return errno;
				}
				buf = nbuf;
				ddptr = buf + (len - space);
			}

			dirp->dd_seek = lseek(fd, (off_t)0, SEEK_CUR);
			n = getdents(fd, ddptr, space);
			/*
			 * For NFS: EINVAL means a bad cookie error
			 * from the server. Keep trying to get a
			 * consistent view, in this case this means
			 * starting all over again.
			 */
			if (n == -1 && errno == EINVAL &&
			    (flags & __DTF_RETRY_ON_BADCOOKIE) != 0) {
				free(buf);
				lseek(fd, (off_t)0, SEEK_SET);
				if (++i > MAXITERATIONS)
					return EINVAL;
				goto retry;
			}
			if (n > 0) {
				ddptr += n;
				space -= n;
			}
		} while (n > 0);

		ddeptr = ddptr;

		/*
		 * Re-open the directory.
		 * This has the effect of rewinding back to the
		 * top of the union stack and is needed by
		 * programs which plan to fchdir to a descriptor
		 * which has also been read -- see fts.c.
		 */
		if (flags & DTF_REWIND) {
			(void) close(fd);
			if ((fd = open(name, O_RDONLY | O_CLOEXEC)) == -1) {
				dirp->dd_buf = buf;
				return errno;
			}
		}

		/*
		 * There is now a buffer full of (possibly) duplicate
		 * names.
		 */
		dirp->dd_buf = buf;

		/*
		 * Go round this loop twice...
		 *
		 * Scan through the buffer, counting entries.
		 * On the second pass, save pointers to each one.
		 * Then sort the pointers and remove duplicate names.
		 */
		if ((flags & DTF_NODUP) != 0) {
			for (dpv = 0;;) {
				for (n = 0, ddptr = buf; ddptr < ddeptr;) {
					struct dirent *dp;

					dp = (struct dirent *)(void *)ddptr;
					if ((long)dp & _DIRENT_ALIGN(dp))
						break;
					/*
					 * d_reclen is unsigned,
					 * so no need to compare <= 0
					 */
					if (dp->d_reclen > (ddeptr + 1 - ddptr))
						break;
					ddptr += dp->d_reclen;
					if (dp->d_fileno) {
						if (dpv)
							dpv[n] = dp;
						n++;
					}
				}

				if (dpv) {
					struct dirent *xp;

					/*
					 * This sort must be stable.
					 */
					mergesort(dpv, (size_t)n, sizeof(*dpv),
					    alphasort);

					dpv[n] = NULL;
					xp = NULL;

					/*
					 * Scan through the buffer in sort
					 * order, zapping the inode number
					 * of any duplicate names.
					 */
					for (n = 0; dpv[n]; n++) {
						struct dirent *dp = dpv[n];

						if ((xp == NULL) ||
						    strcmp(dp->d_name,
						      xp->d_name))
							xp = dp;
						else
							dp->d_fileno = 0;
#ifndef __minix
						if (dp->d_type == DT_WHT &&
						    (flags & DTF_HIDEW))
							dp->d_fileno = 0;
#endif
					}

					free(dpv);
					break;
				} else {
					dpv = malloc((n + 1) *
					    sizeof(struct dirent *));
					if (dpv == NULL)
						break;
				}
			}
		}

		_DIAGASSERT(__type_fit(int, len));
		dirp->dd_len = (int)len;
		dirp->dd_size = ddptr - dirp->dd_buf;
	} else {
		dirp->dd_len = incr;
		dirp->dd_buf = malloc((size_t)dirp->dd_len);
		if (dirp->dd_buf == NULL)
			return errno;
		dirp->dd_seek = 0;
		flags &= ~DTF_REWIND;
	}
	dirp->dd_loc = 0;
	dirp->dd_fd = fd;
	dirp->dd_flags = flags;
	/*
	 * Set up seek point for rewinddir.
	 */
	(void)_telldir_unlocked(dirp);
	return 0;
}

void
_finidir(DIR *dirp)
{
	struct dirpos *poslist;

	free(dirp->dd_buf);

	/* free seekdir/telldir storage */
	for (poslist = dirp->dd_internal; poslist; ) {
		struct dirpos *nextpos = poslist->dp_next;
		free(poslist);
		poslist = nextpos;
	}
	dirp->dd_internal = NULL;
}
