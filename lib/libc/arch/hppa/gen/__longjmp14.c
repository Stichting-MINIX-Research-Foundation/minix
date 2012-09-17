/*	$NetBSD: __longjmp14.c,v 1.5 2012/03/22 12:31:32 skrll Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christian Limpach.
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

#include "namespace.h"
#include <sys/types.h>
#include <ucontext.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define __LIBC12_SOURCE__
#include <setjmp.h>
#include <compat/include/setjmp.h>

#include <stdio.h>
#include <unistd.h>

ssize_t _sys_write(int, void *, size_t);

void
__longjmp14(jmp_buf env, int val)
{
	ucontext_t uc;
	struct sigcontext *sc = (void *)env;
	register_t *regs = (void *)(sc + 1);
	register register_t dp __asm("r27");

	/* Ensure non-zero SP */
	if (sc->sc_sp == 0)
		goto err;

	/* Make return value non-zero */
	if (val == 0)
		val = 1;

	/* Copy callee-saved regs */
	regs -= 3;
	uc.uc_mcontext.__gregs[3] = regs[3];
	uc.uc_mcontext.__gregs[4] = regs[4];
	uc.uc_mcontext.__gregs[5] = regs[5];
	uc.uc_mcontext.__gregs[6] = regs[6];
	uc.uc_mcontext.__gregs[7] = regs[7];
	uc.uc_mcontext.__gregs[8] = regs[8];
	uc.uc_mcontext.__gregs[9] = regs[9];
	uc.uc_mcontext.__gregs[10] = regs[10];
	uc.uc_mcontext.__gregs[11] = regs[11];
	uc.uc_mcontext.__gregs[12] = regs[12];
	uc.uc_mcontext.__gregs[13] = regs[13];
	uc.uc_mcontext.__gregs[14] = regs[14];
	uc.uc_mcontext.__gregs[15] = regs[15];
	uc.uc_mcontext.__gregs[16] = regs[16];
	uc.uc_mcontext.__gregs[17] = regs[17];
	uc.uc_mcontext.__gregs[18] = regs[18];

	/* Preserve the current value of DP */
	/* LINTED dp is r27, so is "initialized" */
	uc.uc_mcontext.__gregs[27] = dp;

	/* Set the desired return value. */
	uc.uc_mcontext.__gregs[_REG_RET0] = val;

	/*
	 * Set _UC_{SET,CLR}STACK according to SS_ONSTACK.
	 *
	 * Restore the signal mask with sigprocmask() instead of _UC_SIGMASK,
	 * since libpthread may want to interpose on signal handling.
	 */
	uc.uc_flags = _UC_CPU | (sc->sc_onstack ? _UC_SETSTACK : _UC_CLRSTACK);

	sigprocmask(SIG_SETMASK, &sc->sc_mask, NULL);

	/* Clear uc_link */
	uc.uc_link = 0;

	/* Copy signal mask */
	uc.uc_sigmask = sc->sc_mask;

	/* Copy special regs */
	uc.uc_mcontext.__gregs[_REG_PSW] = sc->sc_ps;
	uc.uc_mcontext.__gregs[_REG_SP] = sc->sc_sp;
	uc.uc_mcontext.__gregs[_REG_PCSQH] = sc->sc_pcsqh;
	uc.uc_mcontext.__gregs[_REG_PCOQH] = sc->sc_pcoqh;
	uc.uc_mcontext.__gregs[_REG_PCSQT] = sc->sc_pcsqt;
	uc.uc_mcontext.__gregs[_REG_PCOQT] = sc->sc_pcoqt;

	setcontext(&uc);
 err:
	longjmperror();
	abort();
	/* NOTREACHED */
}
