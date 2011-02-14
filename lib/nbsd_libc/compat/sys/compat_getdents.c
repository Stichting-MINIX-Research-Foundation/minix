/*	$NetBSD: compat_getdents.c,v 1.3 2008/04/28 20:22:59 martin Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_getdents.c,v 1.3 2008/04/28 20:22:59 martin Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <dirent.h>
#include <compat/include/dirent.h>
#include <string.h>

/*
 * libc12 compatible getdents routine.
 */
int
getdents(int fd, char *buf, size_t nbytes)
{
	struct dirent *ndp, *nndp, *endp;
	struct dirent12 *odp;
	int rv;

	if ((rv = __getdents30(fd, buf, nbytes)) == -1)
		return rv;

	odp = (struct dirent12 *)(void *)buf;
	ndp = (struct dirent *)(void *)buf;
	endp = (struct dirent *)(void *)&buf[rv];

	/*
	 * In-place conversion. This works because odp
	 * is smaller than ndp, but it has to be done
	 * in the right sequence.
	 */
	for (; ndp < endp; ndp = nndp) {
		nndp = _DIRENT_NEXT(ndp);
		/* XXX: avoid unaligned 64-bit access on sparc64 */
		odp->d_ino = ((u_int32_t *)(void *)&ndp->d_ino)[_QUAD_LOWWORD];
		if (ndp->d_namlen >= sizeof(odp->d_name))
			odp->d_namlen = sizeof(odp->d_name) - 1;
		else
			odp->d_namlen = (u_int8_t)ndp->d_namlen;
		odp->d_type = ndp->d_type;
		(void)memcpy(odp->d_name, ndp->d_name, (size_t)odp->d_namlen);
		odp->d_name[odp->d_namlen] = '\0';
		odp->d_reclen = _DIRENT_SIZE(odp);
		odp = _DIRENT_NEXT(odp);
	}
	return ((char *)(void *)odp) - buf;
}
