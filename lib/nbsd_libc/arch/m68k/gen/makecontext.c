/*	$NetBSD: makecontext.c,v 1.3 2008/04/28 20:22:56 martin Exp $	*/

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
__RCSID("$NetBSD: makecontext.c,v 1.3 2008/04/28 20:22:56 martin Exp $");
#endif

#include <sys/types.h>
#include <inttypes.h>
#include <ucontext.h>
#include "extern.h"

#include <stdarg.h>

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	mcontext_t *mcp = &ucp->uc_mcontext;
	int *sp;
	va_list ap;

	mcp->__gregs[_REG_PC] = (__greg_t)func;

	sp  = (int *)((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp  = (int *)((uintptr_t)sp & ~0x3);	/* Align on word boundary. */
	sp -= (argc + 1);		/* Make room for retaddr and args. */
	mcp->__gregs[_REG_A7] = (__greg_t)sp;
	mcp->__gregs[_REG_A6] = 0;		/* Wipe out frame pointer. */

	*sp++ = (int)_resumecontext;

	va_start(ap, argc);
	while (argc-- > 0)
		*sp++ = va_arg(ap, int);
	va_end(ap);
}
