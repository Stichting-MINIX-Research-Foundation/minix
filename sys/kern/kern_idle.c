/*	$NetBSD: kern_idle.c,v 1.25 2012/01/29 22:55:40 rmind Exp $	*/

/*-
 * Copyright (c)2002, 2006, 2007 YAMAMOTO Takashi,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: kern_idle.c,v 1.25 2012/01/29 22:55:40 rmind Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/idle.h>
#include <sys/kthread.h>
#include <sys/lockdebug.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>	/* uvm_pageidlezero */
#include <uvm/uvm_extern.h>

void
idle_loop(void *dummy)
{
	struct cpu_info *ci = curcpu();
	struct schedstate_percpu *spc;
	struct lwp *l = curlwp;

	kcpuset_atomic_set(kcpuset_running, cpu_index(ci));
	ci->ci_data.cpu_onproc = l;

	/* Update start time for this thread. */
	lwp_lock(l);
	binuptime(&l->l_stime);
	lwp_unlock(l);

	/*
	 * Use spl0() here to ensure that we have the correct interrupt
	 * priority.  This may be the first thread running on the CPU,
	 * in which case we took a dirtbag route to get here.
	 */
	spc = &ci->ci_schedstate;
	(void)splsched();
	spc->spc_flags |= SPCF_RUNNING;
	spl0();

	KERNEL_UNLOCK_ALL(l, NULL);
	l->l_stat = LSONPROC;
	l->l_pflag |= LP_RUNNING;
	for (;;) {
		LOCKDEBUG_BARRIER(NULL, 0);
		KASSERT((l->l_flag & LW_IDLE) != 0);
		KASSERT(ci == curcpu());
		KASSERT(l == curlwp);
		KASSERT(CURCPU_IDLE_P());
		KASSERT(l->l_priority == PRI_IDLE);

		sched_idle();
		if (!sched_curcpu_runnable_p()) {
			if ((spc->spc_flags & SPCF_OFFLINE) == 0) {
				uvm_pageidlezero();
			}
			if (!sched_curcpu_runnable_p()) {
				cpu_idle();
				if (!sched_curcpu_runnable_p() &&
				    !ci->ci_want_resched) {
					continue;
				}
			}
		}
		KASSERT(l->l_mutex == l->l_cpu->ci_schedstate.spc_lwplock);
		lwp_lock(l);
		mi_switch(l);
		KASSERT(curlwp == l);
		KASSERT(l->l_stat == LSONPROC);
	}
}

int
create_idle_lwp(struct cpu_info *ci)
{
	lwp_t *l;
	int error;

	KASSERT(ci->ci_data.cpu_idlelwp == NULL);
	error = kthread_create(PRI_IDLE, KTHREAD_MPSAFE | KTHREAD_IDLE,
	    ci, idle_loop, NULL, &l, "idle/%u", ci->ci_index);
	if (error != 0)
		panic("create_idle_lwp: error %d", error);
	lwp_lock(l);
	l->l_flag |= LW_IDLE;
	lwp_unlock(l);
	l->l_cpu = ci;
	ci->ci_data.cpu_idlelwp = l;

	return error;
}
