/* $NetBSD: disklabel_scan.c,v 1.3 2009/01/18 12:13:03 lukem Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell.
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
#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 2002\
	The NetBSD Foundation, Inc.  All rights reserved.");
__RCSID("$NetBSD: disklabel_scan.c,v 1.3 2009/01/18 12:13:03 lukem Exp $");
#endif

#include <string.h>
#include <unistd.h>
#include <util.h>

#include <sys/disklabel.h>

#define SCAN_INCR	4

int
disklabel_scan(struct disklabel *lp, char *buf, size_t buflen)
{
	size_t	i;

	/* scan for the correct magic numbers. */

	for (i=0; i <= buflen - sizeof(*lp); i += SCAN_INCR) {
		memcpy(lp, buf + i, sizeof(*lp));
		if (lp->d_magic == DISKMAGIC && lp->d_magic2 == DISKMAGIC)
			goto sanity;
	}

	return 1;

sanity:
	/* we've found something, let's sanity check it */
	if (lp->d_npartitions > MAXPARTITIONS || disklabel_dkcksum(lp))
		return 1;

	return 0;
}
