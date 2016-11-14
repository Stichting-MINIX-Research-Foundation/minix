/*	$NetBSD: ast.c,v 1.28 2015/04/17 17:28:33 matt Exp $	*/

/*
 * Copyright (c) 1994,1995 Mark Brinicombe
 * All rights reserved.
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTERS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * ast.c
 *
 * Code to handle ast's and returns to user mode
 *
 * Created      : 11/10/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ast.c,v 1.28 2015/04/17 17:28:33 matt Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#include <sys/userret.h>
#include <sys/vmmeter.h>

#include <arm/locore.h>

#include <uvm/uvm_extern.h>

#ifdef acorn26
#include <machine/machdep.h>
#endif

/*
 * Prototypes
 */
void ast(struct trapframe *);
 
void
userret(struct lwp *l)
{
#if defined(__PROG32) && defined(ARM_MMU_EXTENDED)
	/*
	 * If our ASID got released, access via TTBR0 will have been disabled.
	 * So if it is disabled, activate the lwp again to get a new ASID.
	 */
#ifdef __HAVE_PREEMPTION
	kpreempt_disable();
#endif
	KASSERT(curcpu()->ci_pmap_cur == l->l_proc->p_vmspace->vm_map.pmap);
	if (__predict_false(armreg_ttbcr_read() & TTBCR_S_PD0)) {
		pmap_activate(l);
	}
	KASSERT(!(armreg_ttbcr_read() & TTBCR_S_PD0));
#ifdef __HAVE_PREEMPTION
	kpreempt_enable();
#endif
#endif

	/* Invoke MI userret code */
	mi_userret(l);

#if defined(__PROG32) && defined(DIAGNOSTIC)
	KASSERT(VALID_R15_PSR(lwp_trapframe(l)->tf_pc,
	    lwp_trapframe(l)->tf_spsr));
#endif
}


/*
 * Handle asynchronous system traps.
 * This is called from the irq handler to deliver signals
 * and switch processes if required.
 */

void
ast(struct trapframe *tf)
{
	struct lwp * const l = curlwp;

#ifdef acorn26
	/* Enable interrupts if they were enabled before the trap. */
	if ((tf->tf_r15 & R15_IRQ_DISABLE) == 0)
		int_on();
#else
	/* Interrupts were restored by exception_exit. */
#endif

#ifdef __PROG32
	KASSERT(VALID_R15_PSR(tf->tf_pc, tf->tf_spsr));
#endif

#ifdef __HAVE_PREEMPTION
	kpreempt_disable();
#endif
	struct cpu_info * const ci = curcpu();

	ci->ci_data.cpu_ntrap++;

	KDASSERT(ci->ci_cpl == IPL_NONE);
	const int want_resched = ci->ci_want_resched;
#ifdef __HAVE_PREEMPTION
	kpreempt_enable();
#endif

	if (l->l_pflag & LP_OWEUPC) {
		l->l_pflag &= ~LP_OWEUPC;
		ADDUPROF(l);
	}

	/* Allow a forced task switch. */
	if (want_resched)
		preempt();
	userret(l);
}
