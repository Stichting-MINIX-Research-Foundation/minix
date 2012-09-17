/*	$NetBSD: makecontext.c,v 1.4 2012/03/22 17:32:22 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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
__RCSID("$NetBSD: makecontext.c,v 1.4 2012/03/22 17:32:22 christos Exp $");
#endif

#include <stddef.h>
#include <inttypes.h>
#include <ucontext.h>
#include "extern.h"

#include <stdarg.h>

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	va_list ap;
	int *sp;
	int i;

	/* Compute and align stack pointer. */
	sp = (int *)
	    (((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) & ~0x3);

	/*
	 * Allocate necessary stack space for arguments including arg count
	 * and call frame
	 */
	sp -= argc + 1 + 5;

	va_start(ap, argc);
	sp[5] = argc;
	for (i = 1; argc > 0; argc--, i++)
		sp[5 + i] = va_arg(ap, int);
	va_end(ap);

	sp[0] = 0;			/* condition handler is null */
	sp[1] = 0x20000000;		/* make this a CALLS frame */
	sp[2] = 0;			/* saved argument pointer */
	sp[3] = 0;			/* saved frame pointer */
	sp[4] = (int)(uintptr_t)_resumecontext+2;/* return via trampoline code */

	gr[_REG_AP] = (__greg_t)(uintptr_t)(sp + 5);
	gr[_REG_SP] = (__greg_t)(uintptr_t)sp;
	gr[_REG_FP] = (__greg_t)(uintptr_t)sp;
	gr[_REG_PC] = (__greg_t)(uintptr_t)func+2;

}
