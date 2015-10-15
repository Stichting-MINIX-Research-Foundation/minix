/*	$NetBSD: asysctl.c,v 1.1 2014/06/13 15:45:05 joerg Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 *	All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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
__RCSID("$NetBSD: asysctl.c,v 1.1 2014/06/13 15:45:05 joerg Exp $");

#include "namespace.h"
#include <sys/sysctl.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

__weak_alias(asysctl,_asysctl)
__weak_alias(asysctlbyname,_asysctlbyname)

void *
asysctl(const int *oids, size_t oidlen, size_t *len)
{
	void *data;

	*len = 0;
	data = NULL;

	for (;;) {
		if (sysctl(oids, oidlen, data, len, NULL, 0) == 0) {
			if (*len == 0) {
				free(data);
				return NULL;
			}
			if (data != NULL)
				return data;
			errno = ENOMEM;
		}
		free(data);
		if (errno == ENOMEM && *len != SIZE_MAX) {
			if (*len > SIZE_MAX / 2)
				*len = SIZE_MAX;
			else
				*len *= 2;
			data = malloc(*len);
			if (data == NULL) {
				*len = SIZE_MAX;
				return NULL;
			}
			continue;
		}
		*len = SIZE_MAX;
		return NULL;
	}
}

void *
asysctlbyname(const char *gname, size_t *len)
{
	int name[CTL_MAXNAME];
	u_int namelen;

	if (sysctlgetmibinfo(gname, &name[0], &namelen, NULL, NULL, NULL,
			     SYSCTL_VERSION)) {
		*len = SIZE_MAX;
		return NULL;
	}
	return asysctl(&name[0], namelen, len);
}
