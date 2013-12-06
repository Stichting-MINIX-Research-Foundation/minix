/*	$NetBSD: __longjmp14.c,v 1.7 2013/03/13 08:05:46 martin Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christian Limpach and Matt Thomas.
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
#include <machine/reg.h>
#include <machine/alpha_cpu.h>

#define __LIBC12_SOURCE__
#include <setjmp.h>
#include <compat/include/setjmp.h>

void
__longjmp14(jmp_buf env, int val)
{
	struct sigcontext *sc = (void *)env;
	ucontext_t uc;

	/* Ensure non-zero SP */
	if (sc->sc_sp == 0 || sc->sc_regs[R_ZERO] != 0xacedbade)
		goto err;

	/* Ensure non-zero return value */
	if (val == 0)
		val = -1;

	/* Set _UC_SIGMASK and _UC_CPU */
	uc.uc_flags = _UC_SIGMASK | _UC_CPU;

	/* Clear uc_link */
	uc.uc_link = 0;

	/* Save return value in context */
	uc.uc_mcontext.__gregs[_REG_V0] = val;

	/* Copy saved registers */
	uc.uc_mcontext.__gregs[_REG_S0] = sc->sc_regs[R_S0];
	uc.uc_mcontext.__gregs[_REG_S1] = sc->sc_regs[R_S1];
	uc.uc_mcontext.__gregs[_REG_S2] = sc->sc_regs[R_S2];
	uc.uc_mcontext.__gregs[_REG_S3] = sc->sc_regs[R_S3];
	uc.uc_mcontext.__gregs[_REG_S4] = sc->sc_regs[R_S4];
	uc.uc_mcontext.__gregs[_REG_S5] = sc->sc_regs[R_S5];
	uc.uc_mcontext.__gregs[_REG_S6] = sc->sc_regs[R_S6];
	uc.uc_mcontext.__gregs[_REG_RA] = sc->sc_regs[R_RA];
	uc.uc_mcontext.__gregs[_REG_GP] = sc->sc_regs[R_GP];
	uc.uc_mcontext.__gregs[_REG_SP] = sc->sc_sp;
	uc.uc_mcontext.__gregs[_REG_PC] = sc->sc_pc;
	uc.uc_mcontext.__gregs[_REG_PS] =
	   (sc->sc_ps | ALPHA_PSL_USERSET) & ~ALPHA_PSL_USERCLR;

	/* Copy FP state */
	if (sc->sc_ownedfp) {
		memcpy(&uc.uc_mcontext.__fpregs.__fp_fr,
		    &sc->sc_fpregs, 31 * sizeof(unsigned long));
		uc.uc_mcontext.__fpregs.__fp_fpcr = sc->sc_fpcr;
		/* XXX sc_fp_control */
		uc.uc_flags |= _UC_FPU;
	}

	/* Copy signal mask */
	uc.uc_sigmask = sc->sc_mask;

	setcontext(&uc);
 err:
	longjmperror();
	abort();
	/* NOTREACHED */
}
