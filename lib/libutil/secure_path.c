/*	$NetBSD: secure_path.c,v 1.2 2003/01/06 20:30:30 wiz Exp $	*/

/*-
 * Copyright (c) 1995,1997 Berkeley Software Design, Inc. All rights reserved.
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
 *	This product includes software developed by Berkeley Software Design,
 *	Inc.
 * 4. The name of Berkeley Software Design, Inc.  may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI login_cap.c,v 2.13 1998/02/07 03:17:05 prb Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: secure_path.c,v 1.2 2003/01/06 20:30:30 wiz Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <syslog.h>
#include <util.h>

int
secure_path(const char *path)
{
	struct stat sb;

	_DIAGASSERT(path != NULL);

	/*
	 * If not a regular file, or is owned/writable by someone
	 * other than root, quit.
	 */
	if (lstat(path, &sb) < 0)
		/* syslog(LOG_ERR, "cannot stat %s: %m", path) */;
	else if (!S_ISREG(sb.st_mode))
		syslog(LOG_ERR, "%s: not a regular file", path);
	else if (sb.st_uid != 0)
		syslog(LOG_ERR, "%s: not owned by root", path);
	else if ((sb.st_mode & (S_IWGRP | S_IWOTH)) != 0)
		syslog(LOG_ERR, "%s: writable by non-root", path);
	else
		return (0);

	return (-1);
}
