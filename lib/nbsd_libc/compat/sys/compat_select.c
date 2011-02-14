/*	$NetBSD: compat_select.c,v 1.2 2009/01/11 02:46:26 christos Exp $	*/

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
__RCSID("$NetBSD: compat_select.c,v 1.2 2009/01/11 02:46:26 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <sys/time.h>
#include <compat/sys/time.h>
#include <sys/select.h>
#include <compat/sys/select.h>
#include <sys/poll.h>
#include <compat/sys/poll.h>

__warn_references(pollts,
    "warning: reference to compatibility pollts(); include <poll.h> to generate correct reference")
__warn_references(select,
    "warning: reference to compatibility select(); include <sys/select.h> to generate correct reference")
__warn_references(pselect,
    "warning: reference to compatibility pselect(); include <sys/select.h> to generate correct reference")

#ifdef __weak_alias
__weak_alias(select, _select)
__weak_alias(_sys_select, _select)
__weak_alias(pselect, _pselect)
__weak_alias(_sys_pselect, _pselect)
__weak_alias(pollts, _pollts)
__weak_alias(_sys_pollts, _pollts)
#endif
int
pollts(struct pollfd * __restrict fds, nfds_t nfds,
    const struct timespec50 * __restrict tim50,
    const sigset_t * __restrict sigmask)
{
	struct timespec tim, *timp;

	if (tim50)
		timespec50_to_timespec(tim50, timp = &tim);
	else
		timp = NULL;
	return __pollts50(fds, nfds, timp, sigmask);
}

int
select(int nfds, fd_set * __restrict readfds, fd_set * __restrict writefds,
    fd_set * __restrict exceptfds, struct timeval50 * __restrict tim50)
{
	struct timeval tim, *timp;

	if (tim50)
		timeval50_to_timeval(tim50, timp = &tim);
	else
		timp = NULL;
	return __select50(nfds, readfds, writefds, exceptfds, timp);
}

int
pselect(int nfds, fd_set * __restrict readfds, fd_set * __restrict writefds,
    fd_set * __restrict exceptfds, const struct timespec50 *__restrict tim50,
    const sigset_t * __restrict sigmask)
{
	struct timespec tim, *timp;

	if (tim50)
		timespec50_to_timespec(tim50, timp = &tim);
	else
		timp = NULL;
	return __pselect50(nfds, readfds, writefds, exceptfds, timp, sigmask);
}
