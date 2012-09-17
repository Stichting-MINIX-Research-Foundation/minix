/*	$NetBSD: longjmp.c,v 1.3 2011/04/30 23:41:12 martin Exp $	*/

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
#include <stddef.h>

#define __LIBC12_SOURCE__
#include <setjmp.h>
#include <compat/include/setjmp.h>

struct __jmp_buf_regs_t {
	__greg_t	g4;
	__greg_t	g7;
	__greg_t	save_mask;
};

/*
 * setjmp.S uses hard coded offsets into the jump_buf,
 * make sure any changes cause a compile failure here
 */
__CTASSERT(56 == offsetof(struct __jmp_buf_regs_t,save_mask) +
	sizeof(struct sigcontext));
__CTASSERT(sizeof(sigjmp_buf) >= sizeof(struct __jmp_buf_regs_t) +
	sizeof(struct sigcontext));

/*
 * Use setcontext to reload the stack pointer, program counter <pc,npc>, and
 * set the return value in %o0.  The %i and %l registers will be reloaded
 * from the place to which %sp points.
 */
void
__longjmp14(jmp_buf env, int val)
{
	struct sigcontext *sc = (void *)env;
	struct __jmp_buf_regs_t *r = (void*)&sc[1];
	ucontext_t uc;

	/* Ensure non-zero SP */
	if (sc->sc_sp == 0)
		goto err;

	/* Initialise the context */
	memset(&uc, 0, sizeof(uc));

	/*
	 * Set _UC_{SET,CLR}STACK according to SS_ONSTACK.
	 *
	 * Restore the signal mask with sigprocmask() instead of _UC_SIGMASK,
	 * since libpthread may want to interpose on signal handling.
	 */
	uc.uc_flags = _UC_CPU | (sc->sc_onstack ? _UC_SETSTACK : _UC_CLRSTACK);

	sigprocmask(SIG_SETMASK, &sc->sc_mask, NULL);

	/* Extract PSR, PC, NPC and SP from jmp_buf */
	uc.uc_mcontext.__gregs[_REG_PSR] = sc->sc_psr;
	uc.uc_mcontext.__gregs[_REG_PC] = sc->sc_pc;
	uc.uc_mcontext.__gregs[_REG_nPC] = sc->sc_pc+4;
	uc.uc_mcontext.__gregs[_REG_O6] = sc->sc_sp;
	uc.uc_mcontext.__gregs[_REG_G2] = sc->sc_g1;
	uc.uc_mcontext.__gregs[_REG_G3] = sc->sc_npc;
	uc.uc_mcontext.__gregs[_REG_G4] = r->g4;
	uc.uc_mcontext.__gregs[_REG_G7] = r->g7;

	/* Set the return value; make sure it's non-zero */
	uc.uc_mcontext.__gregs[_REG_O0] = (val != 0 ? val : 1);

	setcontext(&uc);

err:
	longjmperror();
	abort();
	/* NOTREACHED */
}
