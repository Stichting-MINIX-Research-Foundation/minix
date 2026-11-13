/*	$NetBSD: netbsd32_machdep_13.c,v 1.4 2020/07/19 13:55:09 maxv Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: netbsd32_machdep_13.c,v 1.4 2020/07/19 13:55:09 maxv Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_execfmt.h"
#include "opt_mtrr.h"
#endif

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_aout.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/core.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/ras.h>
#include <sys/ptrace.h>
#include <sys/kauth.h>

#include <x86/fpu.h>
#include <x86/dbregs.h>
#include <machine/frame.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#ifdef MTRR
#include <machine/mtrr.h>
#endif
#include <machine/netbsd32_machdep.h>
#include <machine/sysarch.h>
#include <machine/userret.h>

#include <compat/netbsd32/netbsd32.h>
#include <compat/netbsd32/netbsd32_exec.h>
#include <compat/netbsd32/netbsd32_syscallargs.h>

#include <compat/sys/signal.h>
#include <compat/sys/signalvar.h>

int check_sigcontext32(struct lwp *, const struct netbsd32_sigcontext *);

int
compat_13_netbsd32_sigreturn(struct lwp *l, const struct compat_13_netbsd32_sigreturn_args *uap, register_t *retval)
{
	/* {
		syscallarg(struct netbsd32_sigcontext13 *) sigcntxp;
	} */
	struct proc *p = l->l_proc;
	struct netbsd32_sigcontext13 *scp, context;
	struct trapframe *tf;
	sigset_t mask;
	int error;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = (struct netbsd32_sigcontext13 *)NETBSD32PTR64(SCARG(uap, sigcntxp));
	if (copyin((void *)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/* Restore register context. */
	tf = l->l_md.md_regs;

	/*
	 * Check for security violations.
	 */
	error = check_sigcontext32(l, (const struct netbsd32_sigcontext *)&context);
	if (error != 0)
		return error;

	tf->tf_gs = context.sc_gs & 0xFFFF;
	tf->tf_fs = context.sc_fs & 0xFFFF;		
	tf->tf_es = context.sc_es & 0xFFFF;
	tf->tf_ds = context.sc_ds & 0xFFFF;
	tf->tf_rflags = context.sc_eflags;
	tf->tf_rdi = context.sc_edi;
	tf->tf_rsi = context.sc_esi;
	tf->tf_rbp = context.sc_ebp;
	tf->tf_rbx = context.sc_ebx;
	tf->tf_rdx = context.sc_edx;
	tf->tf_rcx = context.sc_ecx;
	tf->tf_rax = context.sc_eax;
	tf->tf_rip = context.sc_eip;
	tf->tf_cs = context.sc_cs & 0xFFFF;
	tf->tf_rsp = context.sc_esp;
	tf->tf_ss = context.sc_ss & 0xFFFF;

	mutex_enter(p->p_lock);
	/* Restore signal stack. */
	if (context.sc_onstack & SS_ONSTACK)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
	else
		l->l_sigstk.ss_flags &= ~SS_ONSTACK;
	/* Restore signal mask. */
	native_sigset13_to_sigset((sigset13_t *)&context.sc_mask, &mask);
	(void) sigprocmask1(l, SIG_SETMASK, &mask, 0);
	mutex_exit(p->p_lock);

	return (EJUSTRETURN);
}

void
netbsd32_machdep_md_13_init(void)
{
 
	/* Nothing to do */
}
 
void
netbsd32_machdep_md_13_fini(void)
{

	/* Nothing to do */
}
