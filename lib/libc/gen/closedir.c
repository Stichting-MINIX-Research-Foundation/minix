/*	$NetBSD: closedir.c,v 1.16 2010/09/26 02:26:59 yamt Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)closedir.c	8.1 (Berkeley) 6/10/93";
#else
__RCSID("$NetBSD: closedir.c,v 1.16 2010/09/26 02:26:59 yamt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"
#include "extern.h"
#include <sys/types.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "dirent_private.h"

#ifdef __weak_alias
__weak_alias(closedir,_closedir)
#endif

/*
 * close a directory.
 */
int
closedir(DIR *dirp)
{
	int fd;

	_DIAGASSERT(dirp != NULL);

#ifdef _REENTRANT
	if (__isthreaded)
		mutex_lock((mutex_t *)dirp->dd_lock);
#endif
	fd = dirp->dd_fd;
	dirp->dd_fd = -1;
	_finidir(dirp);

#ifdef _REENTRANT
	if (__isthreaded) {
		mutex_unlock((mutex_t *)dirp->dd_lock);
		mutex_destroy((mutex_t *)dirp->dd_lock);
		free(dirp->dd_lock);
	}
#endif
	free((void *)dirp);
	return(close(fd));
}
