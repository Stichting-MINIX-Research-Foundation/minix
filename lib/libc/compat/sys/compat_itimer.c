/*	$NetBSD: compat_itimer.c,v 1.2 2009/01/11 02:46:26 christos Exp $	*/

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
__RCSID("$NetBSD: compat_itimer.c,v 1.2 2009/01/11 02:46:26 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <time.h>
#include <compat/include/time.h>
#include <sys/time.h>
#include <compat/sys/time.h>

__warn_references(getitimer,
    "warning: reference to compatibility getitimer(); include <sys/time.h> to generate correct reference")
__warn_references(setitimer,
    "warning: reference to compatibility setitimer(); include <sys/time.h> to generate correct reference")

int
setitimer(int timerid, const struct itimerval50 * __restrict tim50,
    struct itimerval50 * __restrict otim50)
{
	struct itimerval tim, *timp;
	struct itimerval otim, *otimp;
	int error;

	otimp = otim50 ? &otim : NULL;
	if (tim50)
		itimerval50_to_itimerval(tim50, timp = &tim);
	else
		timp = NULL;
	error = __setitimer50(timerid, timp, otimp);
	if (error)
		return error;
	if (otim50)
		itimerval_to_itimerval50(otimp, otim50);
	return 0;
}

int
getitimer(int timerid, struct itimerval50 *tim50)
{
	struct itimerval tim, *timp = tim50 ? &tim : NULL;
	int error = __getitimer50(timerid, timp);
	if (error)
		return error;
	if (tim50)
		itimerval_to_itimerval50(timp, tim50);
	return 0;
}
