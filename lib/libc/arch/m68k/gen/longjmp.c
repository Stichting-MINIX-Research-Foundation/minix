/*	$NetBSD: longjmp.c,v 1.2 2008/04/28 20:22:56 martin Exp $	*/

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

typedef struct {
	__greg_t	__data[6];
	__greg_t	__addr[4];
} __jmp_buf_regs_t;

void
__longjmp14(jmp_buf env, int val)
{
	struct sigcontext *sc = (void *)env;
	__jmp_buf_regs_t *r = (void *)&sc[1];
	ucontext_t uc;

	/* Ensure non-zero SP */
	if (sc->sc_sp == 0)
		goto err;

	/* Make return value non-zero */
	if (val == 0)
		val = 1;

	/* Save return value in context */
	uc.uc_mcontext.__gregs[_REG_D0] = val;

	/*
	 * Set _UC_SIGMASK, _UC_CPU and _UC_M68K_UC_USER
	 * Set _UC_{SET,CLR}STACK according to SS_ONSTACK
	 */
	uc.uc_flags = _UC_SIGMASK | _UC_CPU | _UC_M68K_UC_USER |
		(sc->sc_onstack ? _UC_SETSTACK : _UC_CLRSTACK);

	/* Clear uc_link */
	uc.uc_link = 0;

	/* Copy signal mask */
	uc.uc_sigmask = sc->sc_mask;

	/* Copy SP/PC/PS/FP */
	uc.uc_mcontext.__gregs[_REG_A7] = sc->sc_sp;
	uc.uc_mcontext.__gregs[_REG_PC] = sc->sc_pc;
	uc.uc_mcontext.__gregs[_REG_PS] = sc->sc_ps;
	uc.uc_mcontext.__gregs[_REG_A6] = sc->sc_fp;

	/* Copy remaining non-scratch regs. */
	memcpy(&uc.uc_mcontext.__gregs[_REG_D2],
	    &r->__data, sizeof(r->__data));
	memcpy(&uc.uc_mcontext.__gregs[_REG_A2],
	    &r->__addr, sizeof(r->__addr));

	setcontext(&uc);
 err:
	longjmperror();
	abort();
	/* NOTREACHED */
}
