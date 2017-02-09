/*	$NetBSD: arm_machdep.c,v 1.49 2015/05/02 16:20:41 skrll Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
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
 */

#include "opt_execfmt.h"
#include "opt_cpuoptions.h"
#include "opt_cputypes.h"
#include "opt_arm_debug.h"
#include "opt_multiprocessor.h"
#include "opt_modular.h"

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: arm_machdep.c,v 1.49 2015/05/02 16:20:41 skrll Exp $");

#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/ucontext.h>
#include <sys/evcnt.h>
#include <sys/cpu.h>
#include <sys/atomic.h>
#include <sys/kcpuset.h>

#ifdef EXEC_AOUT
#include <sys/exec_aout.h>
#endif

#include <arm/locore.h>

#include <machine/vmparam.h>

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* from <machine/param.h> */
char	machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */

#ifdef __PROG32
extern const uint32_t undefinedinstruction_bounce[];
#endif

/* Our exported CPU info; we can have only one. */
struct cpu_info cpu_info_store = {
	.ci_cpl = IPL_HIGH,
	.ci_curlwp = &lwp0,
#ifdef __PROG32
	.ci_undefsave[2] = (register_t) undefinedinstruction_bounce,
#if defined(ARM_MMU_EXTENDED) && KERNEL_PID != 0
	.ci_pmap_asid_cur = KERNEL_PID,
#endif
#endif
};

#ifdef MULTIPROCESSOR
struct cpu_info *cpu_info[MAXCPUS] = {
	[0] = &cpu_info_store
};
#endif

const pcu_ops_t * const pcu_ops_md_defs[PCU_UNIT_COUNT] = {
#if defined(FPU_VFP)
	[PCU_FPU] = &arm_vfp_ops,
#endif
};

/*
 * The ARM architecture places the vector page at address 0.
 * Later ARM architecture versions, however, allow it to be
 * relocated to a high address (0xffff0000).  This is primarily
 * to support the Fast Context Switch Extension.
 *
 * This variable contains the address of the vector page.  It
 * defaults to 0; it only needs to be initialized if we enable
 * relocated vectors.
 */
vaddr_t	vector_page;

#if defined(ARM_LOCK_CAS_DEBUG)
/*
 * Event counters for tracking activity of the RAS-based _lock_cas()
 * routine.
 */
struct evcnt _lock_cas_restart =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "_lock_cas", "restart");
EVCNT_ATTACH_STATIC(_lock_cas_restart);

struct evcnt _lock_cas_success =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "_lock_cas", "success");
EVCNT_ATTACH_STATIC(_lock_cas_success);

struct evcnt _lock_cas_fail =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "_lock_cas", "fail");
EVCNT_ATTACH_STATIC(_lock_cas_fail);
#endif /* ARM_LOCK_CAS_DEBUG */

/*
 * Clear registers on exec
 */

void
setregs(struct lwp *l, struct exec_package *pack, vaddr_t stack)
{
	struct trapframe * const tf = lwp_trapframe(l);

	memset(tf, 0, sizeof(*tf));
	tf->tf_r0 = l->l_proc->p_psstrp;
	tf->tf_r12 = stack;			/* needed by pre 1.4 crt0.c */
	tf->tf_usr_sp = stack;
	tf->tf_usr_lr = pack->ep_entry;
	tf->tf_svc_lr = 0x77777777;		/* Something we can see */
	tf->tf_pc = pack->ep_entry;
#ifdef __PROG32
#if defined(__ARMEB__)
	/*
	 * If we are running on ARMv7, we need to set the E bit to force
	 * programs to start as big endian.
	 */
	tf->tf_spsr = PSR_USR32_MODE | (CPU_IS_ARMV7_P() ? PSR_E_BIT : 0);
#else
	tf->tf_spsr = PSR_USR32_MODE;
#endif /* __ARMEB__ */ 

#ifdef THUMB_CODE
	if (pack->ep_entry & 1)
		tf->tf_spsr |= PSR_T_bit;
#endif
#endif /* __PROG32 */

	l->l_md.md_flags = 0;
#ifdef EXEC_AOUT
	if (pack->ep_esch->es_makecmds == exec_aout_makecmds)
		l->l_md.md_flags |= MDLWP_NOALIGNFLT;
#endif
#ifdef FPU_VFP
	vfp_discardcontext(false);
#endif
}

/*
 * startlwp:
 *
 *	Start a new LWP.
 */
void
startlwp(void *arg)
{
	ucontext_t *uc = (ucontext_t *)arg; 
	lwp_t *l = curlwp;
	int error __diagused;

	error = cpu_setmcontext(l, &uc->uc_mcontext, uc->uc_flags);
	KASSERT(error == 0);

	kmem_free(uc, sizeof(ucontext_t));
	userret(l);
}

void
cpu_need_resched(struct cpu_info *ci, int flags)
{
	struct lwp * const l = ci->ci_data.cpu_onproc;
	const bool immed = (flags & RESCHED_IMMED) != 0;
#ifdef MULTIPROCESSOR
	struct cpu_info * const cur_ci = curcpu();
	u_long ipi = IPI_NOP;
#endif

	if (__predict_false((l->l_pflag & LP_INTR) != 0)) {
		/*
		 * No point doing anything, it will switch soon.
		 * Also here to prevent an assertion failure in
		 * kpreempt() due to preemption being set on a
		 * soft interrupt LWP.
		 */
		return;
	}
	if (ci->ci_want_resched && !immed)
		return;

	if (l == ci->ci_data.cpu_idlelwp) {
#ifdef MULTIPROCESSOR
		/*
		 * If the other CPU is idling, it must be waiting for an
		 * event.  So give it one.
		 */
		if (ci != cur_ci)
			goto send_ipi;
#endif
		return;
	}
#ifdef MULTIPROCESSOR
	atomic_swap_uint(&ci->ci_want_resched, 1);
#else
	ci->ci_want_resched = 1;
#endif
	if (flags & RESCHED_KPREEMPT) {
#ifdef __HAVE_PREEMPTION
		atomic_or_uint(&l->l_dopreempt, DOPREEMPT_ACTIVE);
		if (ci == cur_ci) {
			atomic_or_uint(&ci->ci_astpending, __BIT(1));
		} else {
			ipi = IPI_KPREEMPT;
			goto send_ipi;
		}
#endif /* __HAVE_PREEMPTION */
		return;
	}
#ifdef MULTIPROCESSOR
	if (ci == cur_ci || !immed) {
		setsoftast(ci);
		return;
	}
	ipi = IPI_AST;

   send_ipi:
	intr_ipi_send(ci->ci_kcpuset, ipi);
#else
	setsoftast(ci);
#endif /* MULTIPROCESSOR */
}

bool
cpu_intr_p(void)
{
	struct cpu_info * const ci = curcpu();
#ifdef __HAVE_PIC_FAST_SOFTINTS
	if (ci->ci_cpl < IPL_VM)
		return false;
#endif
	return ci->ci_intr_depth != 0;
}

void
ucas_ras_check(trapframe_t *tf)
{
	extern char ucas_32_ras_start[];
	extern char ucas_32_ras_end[];

	if (tf->tf_pc > (vaddr_t)ucas_32_ras_start &&
	    tf->tf_pc < (vaddr_t)ucas_32_ras_end) {
		tf->tf_pc = (vaddr_t)ucas_32_ras_start;
	}
}

#ifdef MODULAR
struct lwp *
arm_curlwp(void)
{
	return curlwp;
}

struct cpu_info *
arm_curcpu(void)
{
	return curcpu();
}
#endif

#ifdef __HAVE_PREEMPTION
void
cpu_set_curpri(int pri)
{
	kpreempt_disable();
	curcpu()->ci_schedstate.spc_curpriority = pri;
	kpreempt_enable();
}

bool
cpu_kpreempt_enter(uintptr_t where, int s)
{
	return s == IPL_NONE;
}

void
cpu_kpreempt_exit(uintptr_t where)
{
	atomic_and_uint(&curcpu()->ci_astpending, (unsigned int)~__BIT(1));
}

bool
cpu_kpreempt_disabled(void)
{
	return curcpu()->ci_cpl != IPL_NONE;
}
#endif /* __HAVE_PREEMPTION */
