/*	$NetBSD: _lwp.c,v 1.7 2012/03/21 00:34:04 christos Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
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
__RCSID("$NetBSD: _lwp.c,v 1.7 2012/03/21 00:34:04 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <ucontext.h>
#include <lwp.h>
#include <stdlib.h>

void
_lwp_makecontext(ucontext_t *u, void (*start)(void *), void *arg,
		 void *private, caddr_t stack_base, size_t stack_size)
{
	__greg_t *gr;
	unsigned long *sp;

	getcontext(u);
	gr = u->uc_mcontext.__gregs;

	u->uc_link = NULL;

	u->uc_stack.ss_sp = stack_base;
	u->uc_stack.ss_size = stack_size;


	sp = (void *)(stack_base + stack_size);
	sp = (ulong *)((ulong)sp & ~0x07);

	/* Make room for the fake caller stack frame (CCFSZ, only in words) */
	sp -= 8 + 8 + 1 + 6 + 1;

	/* Entering (*start)(arg), return is to _lwp_exit */
	gr[_REG_PC] = (ulong) start;
	gr[_REG_nPC] = (ulong) start + 4;
	gr[_REG_O0] = (ulong)arg;
	gr[_REG_O6] = (ulong)sp;
	gr[_REG_O7] = (ulong)_lwp_exit - 8;
	gr[_REG_G7] = (ulong)private;

	/* XXX: uwe: why do we need this? */
	/* create loopback in the window save area on the stack? */
	sp[8+6] = (ulong)sp;		/* %i6 */
	sp[8+7] = (ulong)_lwp_exit - 8;	/* %i7 */
}
