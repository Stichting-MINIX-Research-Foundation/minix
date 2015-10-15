/* $NetBSD: makecontext.c,v 1.1 2014/08/10 05:47:36 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
__RCSID("$NetBSD: makecontext.c,v 1.1 2014/08/10 05:47:36 matt Exp $");
#endif

#include <stddef.h>
#include <inttypes.h>
#include <ucontext.h>
#include "extern.h"

#include <stdarg.h>

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	__greg_t * const gr = ucp->uc_mcontext.__gregs;
	__uint64_t *sp;

	/* Compute and align stack pointer. */
	sp = (uint64_t *)
	    (((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) & -16LL);
	/* Allocate necessary stack space for arguments exceeding x0-7. */
	if (argc > 8)
		sp -= argc - 8;
	gr[_REG_SP] = (__greg_t)(uintptr_t)sp;
	/* Wipe out frame pointer. */
	gr[_REG_X29] = 0;
	/* Arrange for return via the trampoline code. */
	gr[_REG_X30] = (__greg_t)(uintptr_t)_resumecontext;
	gr[_REG_PC] = (__greg_t)(uintptr_t)func;

	va_list ap;
	va_start(ap, argc);

	/* Pass up to four arguments in r0-3. */
	for (int i = 0; i < argc && i < 8; i++) {
		gr[_REG_X0 + i] = va_arg(ap, int);
	}

	/* Pass any additional arguments on the stack. */
	for (int i = 8; i < argc; i++) {
		*sp++ = va_arg(ap, uint64_t);
	}
	va_end(ap);
}
