/*	$NetBSD: compat_sigaltstack.c,v 1.5 2012/03/20 17:06:00 matt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
__RCSID("$NetBSD: compat_sigaltstack.c,v 1.5 2012/03/20 17:06:00 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include <limits.h>
#include <sys/time.h>
#include <compat/sys/time.h>
#include <signal.h>
#include <compat/include/signal.h>
#include <stddef.h>

int
sigaltstack(const struct sigaltstack13 *onss, struct sigaltstack13 *ooss)
{
	stack_t nss, oss;
	int error;

	nss.ss_sp = onss->ss_sp;
	nss.ss_size = onss->ss_size;
	nss.ss_flags = onss->ss_flags;

	error = __sigaltstack14(&nss, &oss);

	if (error == 0 && ooss != NULL) {
		ooss->ss_sp = oss.ss_sp;
		if (oss.ss_size > INT_MAX)
			ooss->ss_size = INT_MAX;
		else
			ooss->ss_size = (int)oss.ss_size;
		ooss->ss_flags = oss.ss_flags;
	}

	return (error);
}
