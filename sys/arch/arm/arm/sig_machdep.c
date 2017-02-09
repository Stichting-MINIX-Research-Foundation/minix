/*	$NetBSD: sig_machdep.c,v 1.49 2015/03/24 08:38:29 matt Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Machine dependent functions for kernel setup
 *
 * Created      : 17/09/94
 */

#include "opt_armfpe.h"

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: sig_machdep.c,v 1.49 2015/03/24 08:38:29 matt Exp $");

#include <sys/mount.h>		/* XXX only needed by syscallargs.h */
#include <sys/cpu.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>
#include <sys/ras.h>
#include <sys/ucontext.h>

#include <arm/locore.h>

#include <machine/pcb.h>
#ifndef acorn26
#include <arm/cpufunc.h>
#endif

void *
getframe(struct lwp *l, int sig, int *onstack)
{
	struct proc * const p = l->l_proc;
	struct trapframe * const tf = lwp_trapframe(l);

	/* Do we need to jump onto the signal stack? */
	*onstack = (l->l_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0
	    && (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;
	if (*onstack)
		return (char *)l->l_sigstk.ss_sp + l->l_sigstk.ss_size;
	return (void *)tf->tf_usr_sp;
}


/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user specified pc.
 */
void
sendsig_siginfo(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp * const l = curlwp;
	struct proc * const p = l->l_proc;
	struct sigacts * const ps = p->p_sigacts;
	struct trapframe * const tf = lwp_trapframe(l);
	struct sigframe_siginfo *fp, frame;
	int onstack, error;
	int sig = ksi->ksi_signo;
	sig_t catcher = SIGACTION(p, sig).sa_handler;

	fp = getframe(l, sig, &onstack);
	
	/* make room on the stack */
	fp--;
	
	/* make the stack aligned */
	fp = (struct sigframe_siginfo *)STACK_ALIGN(fp, STACK_ALIGNBYTES);

	/* populate the siginfo frame */
	frame.sf_si._info = ksi->ksi_info;
	frame.sf_uc.uc_flags = _UC_SIGMASK;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_link = l->l_ctxlink;
	frame.sf_uc.uc_flags |= (l->l_sigstk.ss_flags & SS_ONSTACK)
	    ? _UC_SETSTACK : _UC_CLRSTACK;
	memset(&frame.sf_uc.uc_stack, 0, sizeof(frame.sf_uc.uc_stack));
	sendsig_reset(l, sig);

	mutex_exit(p->p_lock);
	cpu_getmcontext(l, &frame.sf_uc.uc_mcontext, &frame.sf_uc.uc_flags);
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

	/*
	 * Build context to run handler in.  We invoke the handler
	 * directly, only returning via the trampoline.  Note the
	 * trampoline version numbers are coordinated with machine-
	 * dependent code in libc.
	 */
	
	tf->tf_r0 = sig;
	tf->tf_r1 = (int)&fp->sf_si;
	tf->tf_r2 = (int)&fp->sf_uc;

	/* the trampoline uses r5 as the uc address */
	tf->tf_r5 = (int)&fp->sf_uc;
	tf->tf_pc = (int)catcher;
#ifdef THUMB_CODE
	if (((int) catcher) & 1)
		tf->tf_spsr |= PSR_T_bit;
	else
		tf->tf_spsr &= ~PSR_T_bit;
#endif
	tf->tf_usr_sp = (int)fp;
	tf->tf_usr_lr = (int)ps->sa_sigdesc[sig].sd_tramp;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
}

void
cpu_getmcontext(struct lwp *l, mcontext_t *mcp, unsigned int *flags)
{
	struct trapframe * const tf = lwp_trapframe(l);
	__greg_t * const gr = mcp->__gregs;
	__greg_t ras_pc;

	/* Save General Register context. */
	gr[_REG_R0]   = tf->tf_r0;
	gr[_REG_R1]   = tf->tf_r1;
	gr[_REG_R2]   = tf->tf_r2;
	gr[_REG_R3]   = tf->tf_r3;
	gr[_REG_R4]   = tf->tf_r4;
	gr[_REG_R5]   = tf->tf_r5;
	gr[_REG_R6]   = tf->tf_r6;
	gr[_REG_R7]   = tf->tf_r7;
	gr[_REG_R8]   = tf->tf_r8;
	gr[_REG_R9]   = tf->tf_r9;
	gr[_REG_R10]  = tf->tf_r10;
	gr[_REG_R11]  = tf->tf_r11;
	gr[_REG_R12]  = tf->tf_r12;
	gr[_REG_SP]   = tf->tf_usr_sp;
	gr[_REG_LR]   = tf->tf_usr_lr;
	gr[_REG_PC]   = tf->tf_pc;
	gr[_REG_CPSR] = tf->tf_spsr;

	KASSERTMSG(VALID_R15_PSR(gr[_REG_PC], gr[_REG_CPSR]), "%#x %#x",
	    gr[_REG_PC], gr[_REG_CPSR]);

	if ((ras_pc = (__greg_t)ras_lookup(l->l_proc,
	    (void *) gr[_REG_PC])) != -1)
		gr[_REG_PC] = ras_pc;

	*flags |= _UC_CPU;

#ifdef FPU_VFP
	vfp_getcontext(l, mcp, flags);
#endif

	mcp->_mc_tlsbase = (uintptr_t)l->l_private;
	*flags |= _UC_TLSBASE;

#ifdef __PROG32
	const struct pcb * const pcb = lwp_getpcb(l);
	mcp->_mc_user_tpid = pcb->pcb_user_pid_rw;
#endif
}

int
cpu_mcontext_validate(struct lwp *l, const mcontext_t *mcp)
{
	const __greg_t * const gr = mcp->__gregs;

	/* Make sure the processor mode has not been tampered with. */
	if (!VALID_R15_PSR(gr[_REG_PC], gr[_REG_CPSR]))
		return EINVAL;
	return 0;
}

int
cpu_setmcontext(struct lwp *l, const mcontext_t *mcp, unsigned int flags)
{
	struct trapframe * const tf = lwp_trapframe(l);
	const __greg_t * const gr = mcp->__gregs;
	struct proc * const p = l->l_proc;
	int error;

#ifdef FPU_VFP
	if ((flags & _UC_FPU)
	    && (curcpu()->ci_vfp_id == 0 || (flags & _UC_ARM_VFP) == 0))
		return EINVAL;
#endif

	if ((flags & _UC_CPU) != 0) {
		/* Restore General Register context. */
		error = cpu_mcontext_validate(l, mcp);
		if (error)
			return error;

		tf->tf_r0     = gr[_REG_R0];
		tf->tf_r1     = gr[_REG_R1];
		tf->tf_r2     = gr[_REG_R2];
		tf->tf_r3     = gr[_REG_R3];
		tf->tf_r4     = gr[_REG_R4];
		tf->tf_r5     = gr[_REG_R5];
		tf->tf_r6     = gr[_REG_R6];
		tf->tf_r7     = gr[_REG_R7];
		tf->tf_r8     = gr[_REG_R8];
		tf->tf_r9     = gr[_REG_R9];
		tf->tf_r10    = gr[_REG_R10];
		tf->tf_r11    = gr[_REG_R11];
		tf->tf_r12    = gr[_REG_R12];
		tf->tf_usr_sp = gr[_REG_SP];
		tf->tf_usr_lr = gr[_REG_LR];
		tf->tf_pc     = gr[_REG_PC];
		tf->tf_spsr   = gr[_REG_CPSR];
	}

#ifdef FPU_VFP
	if ((flags & _UC_FPU) != 0) {
		/* Restore Floating Point Register context. */
		vfp_setcontext(l, mcp);
	}
#endif

	if ((flags & _UC_TLSBASE) != 0)
		lwp_setprivate(l, (void *)(uintptr_t)mcp->_mc_tlsbase);

	mutex_enter(p->p_lock);
	if (flags & _UC_SETSTACK)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
	if (flags & _UC_CLRSTACK)
		l->l_sigstk.ss_flags &= ~SS_ONSTACK;
	mutex_exit(p->p_lock);

#ifdef __PROG32
	struct pcb * const pcb = lwp_getpcb(l);
	pcb->pcb_user_pid_rw = mcp->_mc_user_tpid;
#endif

	return (0);
}
