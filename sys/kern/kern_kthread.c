/*	$NetBSD: kern_kthread.c,v 1.41 2015/04/21 11:10:29 pooka Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2007, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_kthread.c,v 1.41 2015/04/21 11:10:29 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/sched.h>
#include <sys/kmem.h>

#include <uvm/uvm_extern.h>

static lwp_t *		kthread_jtarget;
static kmutex_t		kthread_lock;
static kcondvar_t	kthread_cv;

void
kthread_sysinit(void)
{

	mutex_init(&kthread_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&kthread_cv, "kthrwait");
	kthread_jtarget = NULL;
}

/*
 * kthread_create: create a kernel thread, that is, system-only LWP.
 */
int
kthread_create(pri_t pri, int flag, struct cpu_info *ci,
    void (*func)(void *), void *arg, lwp_t **lp, const char *fmt, ...)
{
	lwp_t *l;
	vaddr_t uaddr;
	int error, lc;
	va_list ap;

	KASSERT((flag & KTHREAD_INTR) == 0 || (flag & KTHREAD_MPSAFE) != 0);

	uaddr = uvm_uarea_system_alloc(
	   (flag & (KTHREAD_INTR|KTHREAD_IDLE)) == KTHREAD_IDLE ? ci : NULL);
	if (uaddr == 0) {
		return ENOMEM;
	}
	if ((flag & KTHREAD_TS) != 0) {
		lc = SCHED_OTHER;
	} else {
		lc = SCHED_RR;
	}

	error = lwp_create(&lwp0, &proc0, uaddr, LWP_DETACHED, NULL,
	    0, func, arg, &l, lc);
	if (error) {
		uvm_uarea_system_free(uaddr);
		return error;
	}
	if (fmt != NULL) {
		l->l_name = kmem_alloc(MAXCOMLEN, KM_SLEEP);
		va_start(ap, fmt);
		vsnprintf(l->l_name, MAXCOMLEN, fmt, ap);
		va_end(ap);
	}

	/*
	 * Set parameters.
	 */
	if (pri == PRI_NONE) {
		if ((flag & KTHREAD_TS) != 0) {
			/* Maximum user priority level. */
			pri = MAXPRI_USER;
		} else {
			/* Minimum kernel priority level. */
			pri = PRI_KTHREAD;
		}
	}
	mutex_enter(proc0.p_lock);
	lwp_lock(l);
	l->l_priority = pri;
	if (ci != NULL) {
		if (ci != l->l_cpu) {
			lwp_unlock_to(l, ci->ci_schedstate.spc_mutex);
			lwp_lock(l);
		}
		l->l_pflag |= LP_BOUND;
		l->l_cpu = ci;
	}

	if ((flag & KTHREAD_MUSTJOIN) != 0) {
		KASSERT(lp != NULL);
		l->l_pflag |= LP_MUSTJOIN;
	}
	if ((flag & KTHREAD_INTR) != 0) {
		l->l_pflag |= LP_INTR;
	}
	if ((flag & KTHREAD_MPSAFE) == 0) {
		l->l_pflag &= ~LP_MPSAFE;
	}

	/*
	 * Set the new LWP running, unless the caller has requested
	 * otherwise.
	 */
	if ((flag & KTHREAD_IDLE) == 0) {
		l->l_stat = LSRUN;
		sched_enqueue(l, false);
		lwp_unlock(l);
	} else {
		if (ci != NULL)
			lwp_unlock_to(l, ci->ci_schedstate.spc_lwplock);
		else
			lwp_unlock(l);
	}
	mutex_exit(proc0.p_lock);

	/* All done! */
	if (lp != NULL) {
		*lp = l;
	}
	return 0;
}

/*
 * Cause a kernel thread to exit.  Assumes the exiting thread is the
 * current context.
 */
void
kthread_exit(int ecode)
{
	const char *name;
	lwp_t *l = curlwp;

	/* We can't do much with the exit code, so just report it. */
	if (ecode != 0) {
		if ((name = l->l_name) == NULL)
			name = "unnamed";
		printf("WARNING: kthread `%s' (%d) exits with status %d\n",
		    name, l->l_lid, ecode);
	}

	/* Barrier for joining. */
	if (l->l_pflag & LP_MUSTJOIN) {
		mutex_enter(&kthread_lock);
		while (kthread_jtarget != l) {
			cv_wait(&kthread_cv, &kthread_lock);
		}
		kthread_jtarget = NULL;
		cv_broadcast(&kthread_cv);
		mutex_exit(&kthread_lock);
	}

	/* And exit.. */
	lwp_exit(l);
	panic("kthread_exit");
}

/*
 * Wait for a kthread to exit, as pthread_join().
 */
int
kthread_join(lwp_t *l)
{

	KASSERT((l->l_flag & LW_SYSTEM) != 0);

	/*
	 * - Wait if some other thread has occupied the target.
	 * - Specify our kthread as a target and notify it.
	 * - Wait for the target kthread to notify us.
	 */
	mutex_enter(&kthread_lock);
	while (kthread_jtarget) {
		cv_wait(&kthread_cv, &kthread_lock);
	}
	kthread_jtarget = l;
	cv_broadcast(&kthread_cv);
	while (kthread_jtarget == l) {
		cv_wait(&kthread_cv, &kthread_lock);
	}
	mutex_exit(&kthread_lock);

	return 0;
}
