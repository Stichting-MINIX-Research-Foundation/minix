/*	$NetBSD: compat_aio_suspend.c,v 1.2 2009/01/11 02:46:26 christos Exp $ */

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
__RCSID("$NetBSD: compat_aio_suspend.c,v 1.2 2009/01/11 02:46:26 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#define __LIBC12_SOURCE__
#include <time.h>
#include <compat/include/time.h>
#include <compat/include/aio.h>

__warn_references(aio_suspend,
    "warning: reference to compatibility aio_suspend(); include <aio.h> to generate correct reference")

#ifdef __weak_alias
__weak_alias(aio_suspend, _aio_suspend)
__weak_alias(_sys_aio_suspend, _aio_suspend)
#endif
/*
 * Copy timeout to local variable and call the syscall.
 */
int
aio_suspend(const struct aiocb * const list[], int nent,
    const struct timespec50 * ts50)
{
	struct timespec ts, *tsp;

	if (ts50)
		timespec50_to_timespec(ts50, tsp = &ts);
	else
		tsp = NULL;
	return __aio_suspend50(list, nent, tsp);
}
