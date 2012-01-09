/*	$NetBSD: strerror.c,v 1.20 2007/11/24 13:20:57 isaki Exp $	*/

/*-
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

#include <sys/types.h>
#include "saerrno.h"
#include "stand.h"

static const struct mi {
	int	errno;
	const char *msg;
} errlist[] = {
	{ EADAPT,	"bad adaptor number" },
	{ ECTLR,	"bad controller number" },
	{ EUNIT,	"bad drive number" },
	{ EPART,	"bad partition" },
	{ ERDLAB,	"can't read disk label" },
	{ EUNLAB,	"unlabeled" },
	{ ENXIO,	"Device not configured" },
	{ EPERM,	"Operation not permitted" },
	{ ENOENT,	"No such file or directory" },
	{ ESTALE,	"Stale NFS file handle" },
	{ EFTYPE,	"Inappropriate file type or format" },
	{ ENOEXEC,	"Exec format error" },
	{ EIO,		"Input/output error" },
	{ EINVAL,	"Invalid argument" },
	{ ENOTDIR,	"Not a directory" },
	{ EOFFSET,	"invalid file offset" },
	{ EACCES,	"Permission denied" },
	{ 0, 0 },
};

char *
strerror(int err)
{
	static	char ebuf[36];
	const struct mi *mi;

	for (mi = errlist; mi->msg; mi++)
		if (mi->errno == err)
			return __UNCONST(mi->msg);

	snprintf(ebuf, sizeof ebuf, "Unknown error: code %d", err);
	return ebuf;
}
