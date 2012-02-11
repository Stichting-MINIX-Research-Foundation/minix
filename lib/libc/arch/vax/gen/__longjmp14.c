/*	$NetBSD: __longjmp14.c,v 1.4 2008/04/28 20:22:57 martin Exp $	*/

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

#define __LIBC12_SOURCE__
#include <setjmp.h>
#include <compat/include/setjmp.h>

struct _jmp_buf {
	struct sigcontext jb_sc;
	register_t jb_regs[6];
};

void
__longjmp14(jmp_buf env, int val)
{
	struct _jmp_buf *jb = (void *)env;
	ucontext_t uc;

	/* Ensure non-zero SP */
	if (jb->jb_sc.sc_sp == 0)
		goto err;

	/* Ensure non-zero return value */
	if (val == 0)
		val = -1;

	/* Set _UC_SIGMASK and _UC_CPU */
	uc.uc_flags = _UC_SIGMASK | _UC_CPU;

	/* Clear uc_link */
	uc.uc_link = 0;

	/* Save return value in context */
	uc.uc_mcontext.__gregs[_REG_R0] = val;

	/* Copy saved registers */
	uc.uc_mcontext.__gregs[_REG_AP] = jb->jb_sc.sc_ap;
	uc.uc_mcontext.__gregs[_REG_SP] = jb->jb_sc.sc_sp;
	uc.uc_mcontext.__gregs[_REG_FP] = jb->jb_sc.sc_fp;
	uc.uc_mcontext.__gregs[_REG_PC] = jb->jb_sc.sc_pc;
	uc.uc_mcontext.__gregs[_REG_PSL] = jb->jb_sc.sc_ps;

	uc.uc_mcontext.__gregs[_REG_R6] = jb->jb_regs[0];
	uc.uc_mcontext.__gregs[_REG_R7] = jb->jb_regs[1];
	uc.uc_mcontext.__gregs[_REG_R8] = jb->jb_regs[2];
	uc.uc_mcontext.__gregs[_REG_R9] = jb->jb_regs[3];
	uc.uc_mcontext.__gregs[_REG_R10] = jb->jb_regs[4];
	uc.uc_mcontext.__gregs[_REG_R11] = jb->jb_regs[5];

	/* Copy signal mask */
	uc.uc_sigmask = jb->jb_sc.sc_mask;

	setcontext(&uc);
 err:
	longjmperror();
	abort();
	/* NOTREACHED */
}
