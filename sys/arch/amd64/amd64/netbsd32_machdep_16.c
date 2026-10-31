/*	$NetBSD: netbsd32_machdep_16.c,v 1.7 2021/11/06 20:42:56 thorpej Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: netbsd32_machdep_16.c,v 1.7 2021/11/06 20:42:56 thorpej Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_execfmt.h"
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
#include <sys/module_hook.h>

#include <x86/fpu.h>
#include <x86/dbregs.h>
#include <machine/frame.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#include <machine/netbsd32_machdep.h>
#include <machine/sysarch.h>
#include <machine/userret.h>

#include <compat/netbsd32/netbsd32.h>
#include <compat/netbsd32/netbsd32_exec.h>
#include <compat/netbsd32/netbsd32_syscallargs.h>

#include <compat/sys/signal.h>
#include <compat/sys/signalvar.h>

void netbsd32_buildcontext(struct lwp *, struct trapframe *, void *,
    sig_t, int);

int check_sigcontext32(struct lwp *, const struct netbsd32_sigcontext *);

static void
netbsd32_sendsig_sigcontext(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct trapframe *tf;
	int sig = ksi->ksi_signo;
	sig_t catcher = SIGACTION(p, sig).sa_handler;
	struct netbsd32_sigframe_sigcontext *fp, frame;
	int onstack, error;
	struct sigacts *ps = p->p_sigacts;

	tf = l->l_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	onstack =
	    (l->l_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/* Allocate space for the signal handler context. */
	if (onstack)
		fp = (struct netbsd32_sigframe_sigcontext *)
		    ((char *)l->l_sigstk.ss_sp + l->l_sigstk.ss_size);
	else
		fp = (struct netbsd32_sigframe_sigcontext *)tf->tf_rsp;
	fp--;

	memset(&frame, 0, sizeof(frame));

	/* Build stack frame for signal trampoline. */
	switch (ps->sa_sigdesc[sig].sd_vers) {
	case __SIGTRAMP_SIGCODE_VERSION:
		frame.sf_ra = (uint32_t)(u_long)p->p_sigctx.ps_sigcode;
		break;
	case __SIGTRAMP_SIGCONTEXT_VERSION:
		frame.sf_ra = (uint32_t)(u_long)ps->sa_sigdesc[sig].sd_tramp;
		break;
	default:
		/* Don't know what trampoline version; kill it. */
		sigexit(l, SIGILL);
	}
	frame.sf_signum = sig;
	frame.sf_code = ksi->ksi_trap;
	frame.sf_scp = (uint32_t)(u_long)&fp->sf_sc;

	frame.sf_sc.sc_ds = tf->tf_ds & 0xFFFF;
	frame.sf_sc.sc_es = tf->tf_es & 0xFFFF;
	frame.sf_sc.sc_fs = tf->tf_fs & 0xFFFF;
	frame.sf_sc.sc_gs = tf->tf_gs & 0xFFFF;

	frame.sf_sc.sc_eflags = tf->tf_rflags;
	frame.sf_sc.sc_edi = tf->tf_rdi;
	frame.sf_sc.sc_esi = tf->tf_rsi;
	frame.sf_sc.sc_ebp = tf->tf_rbp;
	frame.sf_sc.sc_ebx = tf->tf_rbx;
	frame.sf_sc.sc_edx = tf->tf_rdx;
	frame.sf_sc.sc_ecx = tf->tf_rcx;
	frame.sf_sc.sc_eax = tf->tf_rax;
	frame.sf_sc.sc_eip = tf->tf_rip;
	frame.sf_sc.sc_cs = tf->tf_cs & 0xFFFF;
	frame.sf_sc.sc_esp = tf->tf_rsp;
	frame.sf_sc.sc_ss = tf->tf_ss & 0xFFFF;
	frame.sf_sc.sc_trapno = tf->tf_trapno;
	frame.sf_sc.sc_err = tf->tf_err;

	/* Save signal stack. */
	frame.sf_sc.sc_onstack = l->l_sigstk.ss_flags & SS_ONSTACK;

	/* Save signal mask. */
	frame.sf_sc.sc_mask = *mask;

	sendsig_reset(l, sig);

	mutex_exit(p->p_lock);
	error = copyout(&frame, fp, sizeof(frame));
	mutex_enter(p->p_lock);

	if (error != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	netbsd32_buildcontext(l, tf, fp, catcher, onstack);
}

int
compat_16_netbsd32___sigreturn14(struct lwp *l,
    const struct compat_16_netbsd32___sigreturn14_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(netbsd32_sigcontextp_t) sigcntxp;
	} */
	struct netbsd32_sigcontext *scp, context;
	struct proc *p = l->l_proc;
	struct trapframe *tf;
	int error;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = NETBSD32PTR64(SCARG(uap, sigcntxp));
	if (copyin(scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Check for security violations.
	 */
	error = check_sigcontext32(l, &context);
	if (error != 0)
		return error;

	/* Restore register context. */
	tf = l->l_md.md_regs;
	tf->tf_ds = context.sc_ds & 0xFFFF;
	tf->tf_es = context.sc_es & 0xFFFF;
	cpu_fsgs_reload(l, context.sc_fs, context.sc_gs);
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
	(void) sigprocmask1(l, SIG_SETMASK, &context.sc_mask, 0);
	mutex_exit(p->p_lock);

	return (EJUSTRETURN);
}

void
netbsd32_machdep_md_16_init(void)
{

	MODULE_HOOK_SET(netbsd32_sendsig_sigcontext_16_hook,
	    netbsd32_sendsig_sigcontext);
}

void
netbsd32_machdep_md_16_fini(void)
{

	MODULE_HOOK_UNSET(netbsd32_sendsig_sigcontext_16_hook);
}
