/*	$NetBSD: getdevmajor.c,v 1.5 2009/01/20 20:08:12 drochner Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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
__RCSID("$NetBSD: getdevmajor.c,v 1.5 2009/01/20 20:08:12 drochner Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef __weak_alias
__weak_alias(getdevmajor,_getdevmajor)
#endif

/*
 * XXX temporary alias because getdevmajor() was renamed
 * in -current for some time
 */
dev_t __getdevmajor50(const char *, mode_t);
dev_t
__getdevmajor50(const char *name, mode_t type)
{

	return (dev_t)getdevmajor(name, type);
}

devmajor_t
getdevmajor(const char *name, mode_t type)
{
	struct kinfo_drivers kd[200], *kdp = &kd[0];
	int rc, i;
	size_t sz = sizeof(kd);
	devmajor_t n = NODEVMAJOR;

	if (type != S_IFCHR && type != S_IFBLK) {
		errno = EINVAL;
		return n;
	}

	do {
		rc = sysctlbyname("kern.drivers", kdp, &sz, NULL, 0);
		if (rc == -1) {
			if (errno != ENOMEM)
				goto out;
			if (kdp != &kd[0])
				free(kdp);
			if ((kdp = malloc(sz)) == NULL)
				return n;
		}
	} while (rc == -1);

	rc = sz / sizeof(*kdp);

	for (i = 0; i < rc; i++) {
		if (strcmp(name, kdp[i].d_name) == 0) {
			if (type == S_IFCHR)
				n = kdp[i].d_cmajor;
			else
				n = kdp[i].d_bmajor;
			break;
		}
	}
	if (i >= rc)
		errno = ENOENT;

  out:
	if (kdp != &kd[0])
		free(kdp);

	return n;
}
