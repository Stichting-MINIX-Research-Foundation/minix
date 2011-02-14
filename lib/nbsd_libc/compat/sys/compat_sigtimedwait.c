/*	$NetBSD: compat_sigtimedwait.c,v 1.2 2009/01/11 02:46:27 christos Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__RCSID("$NetBSD: compat_sigtimedwait.c,v 1.2 2009/01/11 02:46:27 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#define __LIBC12_SOURCE__
#include <time.h>
#include <compat/include/time.h>
#include <signal.h>
#include <compat/include/signal.h>

#ifdef __weak_alias
__weak_alias(sigtimedwait, _sigtimedwait)
#endif
/*
 * Copy timeout to local variable and call the syscall.
 */
int
sigtimedwait(const sigset_t * __restrict set, siginfo_t * __restrict info,
    const struct timespec50 * __restrict timeout)
{
	struct timespec ts;

	if (timeout) {
		timespec50_to_timespec(timeout, &ts);
		return ____sigtimedwait50(set, info, &ts);
	} else
		return ____sigtimedwait50(set, info, NULL);
}

int
__sigtimedwait(const sigset_t * __restrict set, siginfo_t * __restrict info,
    struct timespec50 * __restrict timeout)
{
	return sigtimedwait(set, info, timeout);
}
