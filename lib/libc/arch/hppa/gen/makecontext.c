/*	$NetBSD: makecontext.c,v 1.6 2012/03/22 12:31:32 skrll Exp $	*/

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
__RCSID("$NetBSD: makecontext.c,v 1.6 2012/03/22 12:31:32 skrll Exp $");
#endif

#include <inttypes.h>
#include <stddef.h>
#include <ucontext.h>
#include "extern.h"

#include <stdarg.h>

#include <sys/types.h>
#include <machine/frame.h>

void __resumecontext(void) __dead;

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	__greg_t *gp, rp, fp;
	register __greg_t dp __asm("r27");
	uintptr_t *sp;
	int i;
	va_list ap;

	/* LINTED uintptr_t is safe */
	sp  = (uintptr_t *)ucp->uc_stack.ss_sp;
	/* LINTED uintptr_t is safe */
	sp += 16;			/* standard frame */
	sp += (argc >= 4 ? argc : 4);	/* Make room for >=4 arguments. */
	sp  = (uintptr_t *)
	      ((uintptr_t)(sp + 16) & ~0x3f);	/* Align on 64-byte boundary. */

	/* Save away the registers that we'll need. */
	gr[_REG_SP] = (__greg_t)sp;
	rp = (__greg_t)__resumecontext;
	if (rp & 2) {
		gp = (__greg_t *)(rp & ~3);
		rp = gp[0];
		sp[-8] = gp[1];
	}
	gr[_REG_RP] = rp;
	fp = (__greg_t)func;
	if (fp & 2) {
		gp = (__greg_t *)(fp & ~3);
		fp = gp[0];
		gr[_REG_R19] = gp[1];
	}
	gr[_REG_PCOQH] = fp | HPPA_PC_PRIV_USER;
	gr[_REG_PCOQT] = (fp + 4) | HPPA_PC_PRIV_USER;
	/* LINTED dp is reg27, ref. above, so initialized */
	gr[_REG_DP] = dp;

	/* Construct argument list. */
	va_start(ap, argc);
	/* Up to the first four arguments are passed in %arg0-3. */
	for (i = 0; i < argc && i < 4; i++) {
		/* LINTED uintptr_t is safe */
		gr[_REG_ARG0 - i] = va_arg(ap, uintptr_t);
	}
	/* Pass remaining arguments on the stack below the %arg0-3 gap. */
	for (; i < argc; i++) {
		/* LINTED uintptr_t is safe */
		sp[-9 - i] = va_arg(ap, uintptr_t);
	}
	va_end(ap);
}
