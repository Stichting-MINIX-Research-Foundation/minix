/*	$NetBSD: makecontext.c,v 1.7 2011/09/20 08:42:29 joerg Exp $	*/

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
__RCSID("$NetBSD: makecontext.c,v 1.7 2011/09/20 08:42:29 joerg Exp $");
#endif

#include <inttypes.h>
#include <stddef.h>
#include <ucontext.h>
#include "extern.h"

#include <stdarg.h>

void __resumecontext(void) __dead;

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	__greg_t *sp;
	int i;
	va_list ap;

	/* LINTED uintptr_t is safe */
	sp  = (__greg_t *)
	    ((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	/* LINTED uintptr_t is safe */
#if defined(__mips_o32) || defined(__mips_o64)
	sp -= (argc >= 4 ? argc : 4);	/* Make room for >=4 arguments. */
	sp  = (__greg_t *)
	      ((uintptr_t)sp & ~0x7);	/* Align on double-word boundary. */
#elif defined(__mips_n32) || defined(__mips_n64)
	sp -= (argc > 8 ? argc - 8 : 0); /* Make room for > 8 arguments. */
	sp  = (__greg_t *)
	      ((uintptr_t)sp & ~0xf);	/* Align on quad-word boundary. */
#endif

	gr[_REG_SP]  = (intptr_t)sp;
	gr[_REG_RA]  = (intptr_t)__resumecontext;
	gr[_REG_T9]  = (intptr_t)func;		/* required for .abicalls */
	gr[_REG_EPC] = (intptr_t)func;

	/* Construct argument list. */
	va_start(ap, argc);
#if defined(__mips_o32) || defined(__mips_o64)
	/* Up to the first four arguments are passed in $a0-3. */
	for (i = 0; i < argc && i < 4; i++)
		/* LINTED __greg_t is safe */
		gr[_REG_A0 + i] = va_arg(ap, __greg_t);
	/* Pass remaining arguments on the stack above the $a0-3 gap. */
	sp += i;
#endif
#if defined(__mips_n32) || defined(__mips_n64)
	/* Up to the first 8 arguments are passed in $a0-7. */
	for (i = 0; i < argc && i < 8; i++)
		/* LINTED __greg_t is safe */
		gr[_REG_A0 + i] = va_arg(ap, __greg_t);
	/* Pass remaining arguments on the stack above the $a0-3 gap. */
#endif
	/* Pass remaining arguments on the stack above the $a0-3 gap. */
	for (; i < argc; i++)
		/* LINTED uintptr_t is safe */
		*sp++ = va_arg(ap, __greg_t);
	va_end(ap);
}
