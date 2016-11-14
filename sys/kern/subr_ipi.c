/*	$NetBSD: subr_ipi.c,v 1.3 2015/01/18 23:16:35 rmind Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius.
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
 * Inter-processor interrupt (IPI) interface: asynchronous IPIs to
 * invoke functions with a constant argument and synchronous IPIs
 * with the cross-call support.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_ipi.c,v 1.3 2015/01/18 23:16:35 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>
#include <sys/evcnt.h>
#include <sys/cpu.h>
#include <sys/ipi.h>
#include <sys/intr.h>
#include <sys/kcpuset.h>
#include <sys/kmem.h>
#include <sys/lock.h>
#include <sys/mutex.h>

/*
 * An array of the IPI handlers used for asynchronous invocation.
 * The lock protects the slot allocation.
 */

typedef struct {
	ipi_func_t	func;
	void *		arg;
} ipi_intr_t;

static kmutex_t		ipi_mngmt_lock;
static ipi_intr_t	ipi_intrs[IPI_MAXREG]	__cacheline_aligned;

/*
 * Per-CPU mailbox for IPI messages: it is a single cache line storing
 * up to IPI_MSG_MAX messages.  This interface is built on top of the
 * synchronous IPIs.
 */

#define	IPI_MSG_SLOTS	(CACHE_LINE_SIZE / sizeof(ipi_msg_t *))
#define	IPI_MSG_MAX	IPI_MSG_SLOTS

typedef struct {
	ipi_msg_t *	msg[IPI_MSG_SLOTS];
} ipi_mbox_t;


/* Mailboxes for the synchronous IPIs. */
static ipi_mbox_t *	ipi_mboxes	__read_mostly;
static struct evcnt	ipi_mboxfull_ev	__cacheline_aligned;
static void		ipi_msg_cpu_handler(void *);

/* Handler for the synchronous IPIs - it must be zero. */
#define	IPI_SYNCH_ID	0

#ifndef MULTIPROCESSOR
#define	cpu_ipi(ci)	KASSERT(ci == NULL)
#endif

void
ipi_sysinit(void)
{
	const size_t len = ncpu * sizeof(ipi_mbox_t);

	/* Initialise the per-CPU bit fields. */
	for (u_int i = 0; i < ncpu; i++) {
		struct cpu_info *ci = cpu_lookup(i);
		memset(&ci->ci_ipipend, 0, sizeof(ci->ci_ipipend));
	}
	mutex_init(&ipi_mngmt_lock, MUTEX_DEFAULT, IPL_NONE);
	memset(ipi_intrs, 0, sizeof(ipi_intrs));

	/* Allocate per-CPU IPI mailboxes. */
	ipi_mboxes = kmem_zalloc(len, KM_SLEEP);
	KASSERT(ipi_mboxes != NULL);

	/*
	 * Register the handler for synchronous IPIs.  This mechanism
	 * is built on top of the asynchronous interface.  Slot zero is
	 * reserved permanently; it is also handy to use zero as a failure
	 * for other registers (as it is potentially less error-prone).
	 */
	ipi_intrs[IPI_SYNCH_ID].func = ipi_msg_cpu_handler;

	evcnt_attach_dynamic(&ipi_mboxfull_ev, EVCNT_TYPE_MISC, NULL,
	   "ipi", "full");
}

/*
 * ipi_register: register an asynchronous IPI handler.
 *
 * => Returns IPI ID which is greater than zero; on failure - zero.
 */
u_int
ipi_register(ipi_func_t func, void *arg)
{
	mutex_enter(&ipi_mngmt_lock);
	for (u_int i = 0; i < IPI_MAXREG; i++) {
		if (ipi_intrs[i].func == NULL) {
			/* Register the function. */
			ipi_intrs[i].func = func;
			ipi_intrs[i].arg = arg;
			mutex_exit(&ipi_mngmt_lock);

			KASSERT(i != IPI_SYNCH_ID);
			return i;
		}
	}
	mutex_exit(&ipi_mngmt_lock);
	printf("WARNING: ipi_register: table full, increase IPI_MAXREG\n");
	return 0;
}

/*
 * ipi_unregister: release the IPI handler given the ID.
 */
void
ipi_unregister(u_int ipi_id)
{
	ipi_msg_t ipimsg = { .func = (ipi_func_t)nullop };

	KASSERT(ipi_id != IPI_SYNCH_ID);
	KASSERT(ipi_id < IPI_MAXREG);

	/* Release the slot. */
	mutex_enter(&ipi_mngmt_lock);
	KASSERT(ipi_intrs[ipi_id].func != NULL);
	ipi_intrs[ipi_id].func = NULL;

	/* Ensure that there are no IPIs in flight. */
	kpreempt_disable();
	ipi_broadcast(&ipimsg);
	ipi_wait(&ipimsg);
	kpreempt_enable();
	mutex_exit(&ipi_mngmt_lock);
}

/*
 * ipi_trigger: asynchronously send an IPI to the specified CPU.
 */
void
ipi_trigger(u_int ipi_id, struct cpu_info *ci)
{
	const u_int i = ipi_id >> IPI_BITW_SHIFT;
	const uint32_t bitm = 1U << (ipi_id & IPI_BITW_MASK);

	KASSERT(ipi_id < IPI_MAXREG);
	KASSERT(kpreempt_disabled());
	KASSERT(curcpu() != ci);

	/* Mark as pending and send an IPI. */
	if (membar_consumer(), (ci->ci_ipipend[i] & bitm) == 0) {
		atomic_or_32(&ci->ci_ipipend[i], bitm);
		cpu_ipi(ci);
	}
}

/*
 * ipi_trigger_multi: same as ipi_trigger() but sends to the multiple
 * CPUs given the target CPU set.
 */
void
ipi_trigger_multi(u_int ipi_id, const kcpuset_t *target)
{
	const cpuid_t selfid = cpu_index(curcpu());
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	KASSERT(kpreempt_disabled());
	KASSERT(target != NULL);

	for (CPU_INFO_FOREACH(cii, ci)) {
		const cpuid_t cpuid = cpu_index(ci);

		if (!kcpuset_isset(target, cpuid) || cpuid == selfid) {
			continue;
		}
		ipi_trigger(ipi_id, ci);
	}
	if (kcpuset_isset(target, selfid)) {
		int s = splhigh();
		ipi_cpu_handler();
		splx(s);
	}
}

/*
 * put_msg: insert message into the mailbox.
 */
static inline void
put_msg(ipi_mbox_t *mbox, ipi_msg_t *msg)
{
	int count = SPINLOCK_BACKOFF_MIN;
again:
	for (u_int i = 0; i < IPI_MSG_MAX; i++) {
		if (__predict_true(mbox->msg[i] == NULL) &&
		    atomic_cas_ptr(&mbox->msg[i], NULL, msg) == NULL) {
			return;
		}
	}

	/* All slots are full: we have to spin-wait. */
	ipi_mboxfull_ev.ev_count++;
	SPINLOCK_BACKOFF(count);
	goto again;
}

/*
 * ipi_cpu_handler: the IPI handler.
 */
void
ipi_cpu_handler(void)
{
	struct cpu_info * const ci = curcpu();

	/*
	 * Handle asynchronous IPIs: inspect per-CPU bit field, extract
	 * IPI ID numbers and execute functions in those slots.
	 */
	for (u_int i = 0; i < IPI_BITWORDS; i++) {
		uint32_t pending, bit;

		if (ci->ci_ipipend[i] == 0) {
			continue;
		}
		pending = atomic_swap_32(&ci->ci_ipipend[i], 0);
#ifndef __HAVE_ATOMIC_AS_MEMBAR
		membar_producer();
#endif
		while ((bit = ffs(pending)) != 0) {
			const u_int ipi_id = (i << IPI_BITW_SHIFT) | --bit;
			ipi_intr_t *ipi_hdl = &ipi_intrs[ipi_id];

			pending &= ~(1U << bit);
			KASSERT(ipi_hdl->func != NULL);
			ipi_hdl->func(ipi_hdl->arg);
		}
	}
}

/*
 * ipi_msg_cpu_handler: handle synchronous IPIs - iterate mailbox,
 * execute the passed functions and acknowledge the messages.
 */
static void
ipi_msg_cpu_handler(void *arg __unused)
{
	const struct cpu_info * const ci = curcpu();
	ipi_mbox_t *mbox = &ipi_mboxes[cpu_index(ci)];

	for (u_int i = 0; i < IPI_MSG_MAX; i++) {
		ipi_msg_t *msg;

		/* Get the message. */
		if ((msg = mbox->msg[i]) == NULL) {
			continue;
		}
		mbox->msg[i] = NULL;

		/* Execute the handler. */
		KASSERT(msg->func);
		msg->func(msg->arg);

		/* Ack the request. */
		atomic_dec_uint(&msg->_pending);
	}
}

/*
 * ipi_unicast: send an IPI to a single CPU.
 *
 * => The CPU must be remote; must not be local.
 * => The caller must ipi_wait() on the message for completion.
 */
void
ipi_unicast(ipi_msg_t *msg, struct cpu_info *ci)
{
	const cpuid_t id = cpu_index(ci);

	KASSERT(msg->func != NULL);
	KASSERT(kpreempt_disabled());
	KASSERT(curcpu() != ci);

	msg->_pending = 1;
	membar_producer();

	put_msg(&ipi_mboxes[id], msg);
	ipi_trigger(IPI_SYNCH_ID, ci);
}

/*
 * ipi_multicast: send an IPI to each CPU in the specified set.
 *
 * => The caller must ipi_wait() on the message for completion.
 */
void
ipi_multicast(ipi_msg_t *msg, const kcpuset_t *target)
{
	const struct cpu_info * const self = curcpu();
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	u_int local;

	KASSERT(msg->func != NULL);
	KASSERT(kpreempt_disabled());

	local = !!kcpuset_isset(target, cpu_index(self));
	msg->_pending = kcpuset_countset(target) - local;
	membar_producer();

	for (CPU_INFO_FOREACH(cii, ci)) {
		cpuid_t id;

		if (__predict_false(ci == self)) {
			continue;
		}
		id = cpu_index(ci);
		if (!kcpuset_isset(target, id)) {
			continue;
		}
		put_msg(&ipi_mboxes[id], msg);
		ipi_trigger(IPI_SYNCH_ID, ci);
	}
	if (local) {
		msg->func(msg->arg);
	}
}

/*
 * ipi_broadcast: send an IPI to all CPUs.
 *
 * => The caller must ipi_wait() on the message for completion.
 */
void
ipi_broadcast(ipi_msg_t *msg)
{
	const struct cpu_info * const self = curcpu();
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	KASSERT(msg->func != NULL);
	KASSERT(kpreempt_disabled());

	msg->_pending = ncpu - 1;
	membar_producer();

	/* Broadcast IPIs for remote CPUs. */
	for (CPU_INFO_FOREACH(cii, ci)) {
		cpuid_t id;

		if (__predict_false(ci == self)) {
			continue;
		}
		id = cpu_index(ci);
		put_msg(&ipi_mboxes[id], msg);
		ipi_trigger(IPI_SYNCH_ID, ci);
	}

	/* Finally, execute locally. */
	msg->func(msg->arg);
}

/*
 * ipi_wait: spin-wait until the message is processed.
 */
void
ipi_wait(ipi_msg_t *msg)
{
	int count = SPINLOCK_BACKOFF_MIN;

	while (msg->_pending) {
		KASSERT(msg->_pending < ncpu);
		SPINLOCK_BACKOFF(count);
	}
}
