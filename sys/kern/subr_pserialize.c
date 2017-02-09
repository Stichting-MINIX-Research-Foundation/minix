/*	$NetBSD: subr_pserialize.c,v 1.8 2015/06/12 19:18:30 dholland Exp $	*/

/*-
 * Copyright (c) 2010, 2011 The NetBSD Foundation, Inc.
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

/*
 * Passive serialization.
 *
 * Implementation accurately matches the lapsed US patent 4809168, therefore
 * code is patent-free in the United States.  Your use of this code is at
 * your own risk.
 * 
 * Note for NetBSD developers: all changes to this source file must be
 * approved by the <core>.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_pserialize.c,v 1.8 2015/06/12 19:18:30 dholland Exp $");

#include <sys/param.h>

#include <sys/condvar.h>
#include <sys/cpu.h>
#include <sys/evcnt.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/pserialize.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/xcall.h>

struct pserialize {
	TAILQ_ENTRY(pserialize)	psz_chain;
	lwp_t *			psz_owner;
	kcpuset_t *		psz_target;
	kcpuset_t *		psz_pass;
};

static u_int			psz_work_todo	__cacheline_aligned;
static kmutex_t			psz_lock	__cacheline_aligned;
static struct evcnt		psz_ev_excl	__cacheline_aligned;

/*
 * As defined in "Method 1":
 *	q0: "0 MP checkpoints have occured".
 *	q1: "1 MP checkpoint has occured".
 *	q2: "2 MP checkpoints have occured".
 */
static TAILQ_HEAD(, pserialize)	psz_queue0	__cacheline_aligned;
static TAILQ_HEAD(, pserialize)	psz_queue1	__cacheline_aligned;
static TAILQ_HEAD(, pserialize)	psz_queue2	__cacheline_aligned;

/*
 * pserialize_init:
 *
 *	Initialize passive serialization structures.
 */
void
pserialize_init(void)
{

	psz_work_todo = 0;
	TAILQ_INIT(&psz_queue0);
	TAILQ_INIT(&psz_queue1);
	TAILQ_INIT(&psz_queue2);
	mutex_init(&psz_lock, MUTEX_DEFAULT, IPL_SCHED);
	evcnt_attach_dynamic(&psz_ev_excl, EVCNT_TYPE_MISC, NULL,
	    "pserialize", "exclusive access");
}

/*
 * pserialize_create:
 *
 *	Create and initialize a passive serialization object.
 */
pserialize_t
pserialize_create(void)
{
	pserialize_t psz;

	psz = kmem_zalloc(sizeof(struct pserialize), KM_SLEEP);
	kcpuset_create(&psz->psz_target, true);
	kcpuset_create(&psz->psz_pass, true);
	psz->psz_owner = NULL;

	return psz;
}

/*
 * pserialize_destroy:
 *
 *	Destroy a passive serialization object.
 */
void
pserialize_destroy(pserialize_t psz)
{

	KASSERT(psz->psz_owner == NULL);

	kcpuset_destroy(psz->psz_target);
	kcpuset_destroy(psz->psz_pass);
	kmem_free(psz, sizeof(struct pserialize));
}

/*
 * pserialize_perform:
 *
 *	Perform the write side of passive serialization.  The calling
 *	thread holds an exclusive lock on the data object(s) being updated.
 *	We wait until every processor in the system has made at least two
 *	passes through cpu_switchto().  The wait is made with the caller's
 *	update lock held, but is short term.
 */
void
pserialize_perform(pserialize_t psz)
{
	uint64_t xc;

	KASSERT(!cpu_intr_p());
	KASSERT(!cpu_softintr_p());

	if (__predict_false(panicstr != NULL)) {
		return;
	}
	KASSERT(psz->psz_owner == NULL);
	KASSERT(ncpu > 0);

	/*
	 * Set up the object and put it onto the queue.  The lock
	 * activity here provides the necessary memory barrier to
	 * make the caller's data update completely visible to
	 * other processors.
	 */
	psz->psz_owner = curlwp;
	kcpuset_copy(psz->psz_target, kcpuset_running);
	kcpuset_zero(psz->psz_pass);

	mutex_spin_enter(&psz_lock);
	TAILQ_INSERT_TAIL(&psz_queue0, psz, psz_chain);
	psz_work_todo++;

	do {
		mutex_spin_exit(&psz_lock);

		/*
		 * Force some context switch activity on every CPU, as
		 * the system may not be busy.  Pause to not flood.
		 */
		xc = xc_broadcast(XC_HIGHPRI, (xcfunc_t)nullop, NULL, NULL);
		xc_wait(xc);
		kpause("psrlz", false, 1, NULL);

		mutex_spin_enter(&psz_lock);
	} while (!kcpuset_iszero(psz->psz_target));

	psz_ev_excl.ev_count++;
	mutex_spin_exit(&psz_lock);

	psz->psz_owner = NULL;
}

int
pserialize_read_enter(void)
{

	KASSERT(!cpu_intr_p());
	return splsoftserial();
}

void
pserialize_read_exit(int s)
{

	splx(s);
}

/*
 * pserialize_switchpoint:
 *
 *	Monitor system context switch activity.  Called from machine
 *	independent code after mi_switch() returns.
 */ 
void
pserialize_switchpoint(void)
{
	pserialize_t psz, next;
	cpuid_t cid;

	/*
	 * If no updates pending, bail out.  No need to lock in order to
	 * test psz_work_todo; the only ill effect of missing an update
	 * would be to delay LWPs waiting in pserialize_perform().  That
	 * will not happen because updates are on the queue before an
	 * xcall is generated (serialization) to tickle every CPU.
	 */
	if (__predict_true(psz_work_todo == 0)) {
		return;
	}
	mutex_spin_enter(&psz_lock);
	cid = cpu_index(curcpu());

	/*
	 * At first, scan through the second queue and update each request,
	 * if passed all processors, then transfer to the third queue. 
	 */
	for (psz = TAILQ_FIRST(&psz_queue1); psz != NULL; psz = next) {
		next = TAILQ_NEXT(psz, psz_chain);
		kcpuset_set(psz->psz_pass, cid);
		if (!kcpuset_match(psz->psz_pass, psz->psz_target)) {
			continue;
		}
		kcpuset_zero(psz->psz_pass);
		TAILQ_REMOVE(&psz_queue1, psz, psz_chain);
		TAILQ_INSERT_TAIL(&psz_queue2, psz, psz_chain);
	}
	/*
	 * Scan through the first queue and update each request,
	 * if passed all processors, then move to the second queue. 
	 */
	for (psz = TAILQ_FIRST(&psz_queue0); psz != NULL; psz = next) {
		next = TAILQ_NEXT(psz, psz_chain);
		kcpuset_set(psz->psz_pass, cid);
		if (!kcpuset_match(psz->psz_pass, psz->psz_target)) {
			continue;
		}
		kcpuset_zero(psz->psz_pass);
		TAILQ_REMOVE(&psz_queue0, psz, psz_chain);
		TAILQ_INSERT_TAIL(&psz_queue1, psz, psz_chain);
	}
	/*
	 * Process the third queue: entries have been seen twice on every
	 * processor, remove from the queue and notify the updating thread.
	 */
	while ((psz = TAILQ_FIRST(&psz_queue2)) != NULL) {
		TAILQ_REMOVE(&psz_queue2, psz, psz_chain);
		kcpuset_zero(psz->psz_target);
		psz_work_todo--;
	}
	mutex_spin_exit(&psz_lock);
}
