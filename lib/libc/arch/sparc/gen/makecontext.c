/*	$NetBSD: makecontext.c,v 1.3 2008/04/28 20:22:57 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: makecontext.c,v 1.3 2008/04/28 20:22:57 martin Exp $");
#endif

#include <inttypes.h>
#include <stddef.h>
#include <ucontext.h>
#include "extern.h"

#include <stdarg.h>

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	int i;
	unsigned long *sp;
	va_list ap;

	sp = (unsigned long *)
	    ((unsigned long)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	/* Make room for: rwindow, struct return pointer, argd, argx */
	sp -= 8 + 8 + 1 + 6 + 1; /* CCFSZ, only in words */
	/* CCFSZ provides space for up to 7 arguments, add more if necessary */
	if (argc > 7)
		sp -= argc - 7;
	/* Align on double-word boundary. */
	sp = (unsigned long *)((unsigned long)sp & ~0x7);

	gr[_REG_O6] = (__greg_t)sp;
	gr[_REG_PC] = (__greg_t)func;
	gr[_REG_nPC] = (__greg_t)func + 4;
	gr[_REG_O7] = (__greg_t)_resumecontext - 8;

	va_start(ap, argc);
	/* Pass up to 6 arguments in %o0..%o5. */
	for (i = 0; i < argc && i < 6; i++)
		gr[_REG_O0 + i] = va_arg(ap, unsigned long);
	/* Pass any additional arguments on the stack. */
	for (/* i = 6 */; i < argc; i++)
		sp[8 + 8 + 1 + 6 + (i - 6)] = va_arg(ap, unsigned long);
	va_end(ap);
}
