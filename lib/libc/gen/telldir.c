/*	$NetBSD: telldir.c,v 1.19 2008/05/04 18:53:26 tonnerre Exp $	*/

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
static char sccsid[] = "@(#)telldir.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: telldir.c,v 1.19 2008/05/04 18:53:26 tonnerre Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"

#ifdef __minix
#include <sys/types.h>
#endif

#include "extern.h"
#include <sys/param.h>

#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

#include "dirent_private.h"

#ifdef __weak_alias
__weak_alias(telldir,_telldir)
#endif

long
telldir(DIR *dirp)
{
	long rv;
#ifdef _REENTRANT
	if (__isthreaded) {
		mutex_lock((mutex_t *)dirp->dd_lock);
		rv = (intptr_t)_telldir_unlocked(dirp);
		mutex_unlock((mutex_t *)dirp->dd_lock);
	} else
#endif
		rv = (intptr_t)_telldir_unlocked(dirp);
	return rv;
}

/*
 * return a pointer into a directory
 */
long
_telldir_unlocked(DIR *dirp)
{
	struct dirpos *lp;

	for (lp = dirp->dd_internal; lp; lp = lp->dp_next)
		if (lp->dp_seek == dirp->dd_seek &&
		    lp->dp_loc == dirp->dd_loc)
			return (intptr_t)lp;

	if ((lp = malloc(sizeof(*lp))) == NULL)
		return (-1);

	lp->dp_seek = dirp->dd_seek;
	lp->dp_loc = dirp->dd_loc;
	lp->dp_next = dirp->dd_internal;
	dirp->dd_internal = lp;

	return (intptr_t)lp;
}

/*
 * seek to an entry in a directory.
 * Only values returned by "telldir" should be passed to seekdir.
 */
void
_seekdir_unlocked(DIR *dirp, long loc)
{
	struct dirpos *lp;

	_DIAGASSERT(dirp != NULL);

	for (lp = dirp->dd_internal; lp; lp = lp->dp_next)
		if ((intptr_t)lp == loc)
			break;

	if (lp == NULL)
		return;

	if (lp->dp_loc == dirp->dd_loc && lp->dp_seek == dirp->dd_seek)
		return;

	dirp->dd_seek = lseek(dirp->dd_fd, lp->dp_seek, SEEK_SET);
	dirp->dd_loc = 0;
	while (dirp->dd_loc < lp->dp_loc)
		if (_readdir_unlocked(dirp, 0) == NULL)
			break;
}
