/*	$NetBSD: thread-stub-init.c,v 1.2 2013/08/19 22:14:37 matt Exp $	*/

/*-
 * Copyright (c) 2003, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
__RCSID("$NetBSD: thread-stub-init.c,v 1.2 2013/08/19 22:14:37 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#ifdef _REENTRANT
#define __LIBC_THREAD_STUBS
#include "reentrant.h"
#include <signal.h>

/* libpthread init */

__weak_alias(__libc_thr_init,__libc_thr_init_stub)

void __section(".text.startup")
__libc_thr_init_stub(void)
{

	/* nothing, may be overridden by libpthread */
}

#define DIE()   (void)raise(SIGABRT)

__weak_alias(__libc_thr_errno,__libc_thr_errno_stub)
int *
__libc_thr_errno_stub(void)
{
	DIE();

	return (NULL);
}
#endif /* _REENTRANT */
