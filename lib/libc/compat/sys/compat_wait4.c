/*	$NetBSD: compat_wait4.c,v 1.3 2015/03/26 11:17:08 justin Exp $	*/

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
__RCSID("$NetBSD: compat_wait4.c,v 1.3 2015/03/26 11:17:08 justin Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <compat/sys/wait.h>
#include <sys/resource.h>
#include <compat/sys/resource.h>

__warn_references(wait3,
    "warning: reference to compatibility wait3(); include <sys/wait.h> to generate correct reference")
__warn_references(wait4,
    "warning: reference to compatibility wait4(); include <sys/wait.h> to generate correct reference")

extern void __rusage_to_rusage50(const struct rusage *, struct rusage50 *);

#ifdef __weak_alias
__weak_alias(wait4, _wait4)
__weak_alias(_sys_wait4, _wait4)
__weak_alias(wait3, _wait3)
#endif

/*
 * libc12 compatible wait4 routine.
 */
pid_t
wait3(int *status, int options, struct rusage50 *ru50)
{
	struct rusage ru;
	pid_t rv;

	if ((rv = __wait350(status, options, ru50 ? &ru : NULL)) == -1)
		return rv;
	if (ru50)
		__rusage_to_rusage50(&ru, ru50);
	return rv;
}

pid_t
wait4(pid_t wpid, int *status, int options, struct rusage50 *ru50)
{
	struct rusage ru;
	pid_t rv;

	if ((rv = __wait450(wpid, status, options, ru50 ? &ru : NULL)) == -1)
		return rv;
	if (ru50)
		__rusage_to_rusage50(&ru, ru50);
	return rv;
}
