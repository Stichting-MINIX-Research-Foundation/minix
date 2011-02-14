/*	$NetBSD: compat_getrusage.c,v 1.2 2009/01/11 02:46:26 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: compat_getrusage.c,v 1.2 2009/01/11 02:46:26 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <compat/sys/resource.h>

__warn_references(getrusage,
    "warning: reference to compatibility getrusage(); include <sys/resource.h> to generate correct reference")

void __rusage_to_rusage50(const struct rusage *, struct rusage50 *);

void
__rusage_to_rusage50(const struct rusage *ru, struct rusage50 *ru50)
{
	(void)memcpy(&ru50->ru_first, &ru->ru_first,
	    (size_t)((char *)(void *)&ru50->ru_last -
	    (char *)(void *)&ru50->ru_first));
	ru50->ru_maxrss = ru->ru_maxrss;
	timeval_to_timeval50(&ru->ru_utime, &ru50->ru_utime);
	timeval_to_timeval50(&ru->ru_stime, &ru50->ru_stime);
}

/*
 * libc12 compatible getrusage routine.
 */
int
getrusage(int who, struct rusage50 *ru50)
{
	struct rusage ru;
	int rv;

	if ((rv = __getrusage50(who, &ru)) == -1)
		return rv;
	__rusage_to_rusage50(&ru, ru50);
	return rv;
}
