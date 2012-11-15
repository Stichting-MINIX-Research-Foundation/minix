/*	$NetBSD: _lwp.c,v 1.8 2012/08/31 20:57:24 drochner Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams.
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
__RCSID("$NetBSD: _lwp.c,v 1.8 2012/08/31 20:57:24 drochner Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <inttypes.h>
#include <ucontext.h>
#include <lwp.h>
#include <stdlib.h>

void
_lwp_makecontext(ucontext_t *u, void (*start)(void *),
    void *arg, void *private, caddr_t stack_base, size_t stack_size)
{
	void **sp;

	getcontext(u);
	u->uc_link = NULL;

	u->uc_stack.ss_sp = stack_base;
	u->uc_stack.ss_size = stack_size;

	/* LINTED uintptr_t is safe */
	u->uc_mcontext.__gregs[_REG_EIP] = (uintptr_t)start;
	
	/* Align to a 16-byte boundary for SSE */
	/* LINTED uintptr_t is safe */
	sp = (void **) (((uintptr_t)(stack_base + stack_size - 4) & ~0xf) + 4);
	
	*--sp = arg;
	*--sp = (void *) _lwp_exit;
	
	/* LINTED uintptr_t is safe */
	u->uc_mcontext.__gregs[_REG_UESP] = (uintptr_t) sp;

	u->uc_mcontext._mc_tlsbase = (uintptr_t)private;
	u->uc_flags |= _UC_TLSBASE;
}
