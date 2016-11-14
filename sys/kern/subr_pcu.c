/*	$NetBSD: subr_pcu.c,v 1.19 2014/05/25 14:53:55 rmind Exp $	*/

/*-
 * Copyright (c) 2011, 2014 The NetBSD Foundation, Inc.
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
 * Per CPU Unit (PCU) - is an interface to manage synchronization of any
 * per CPU context (unit) tied with LWP context.  Typical use: FPU state.
 *
 * Concurrency notes:
 *
 *	PCU state may be loaded only by the current LWP, that is, curlwp.
 *	Therefore, only LWP itself can set a CPU for lwp_t::l_pcu_cpu[id].
 *
 *	There are some important rules about operation calls.  The request
 *	for a PCU release can be from a) the owner LWP (regardless whether
 *	the PCU state is on the current CPU or remote CPU) b) any other LWP
 *	running on that CPU (in such case, the owner LWP is on a remote CPU
 *	or sleeping).
 *
 *	In any case, the PCU state can *only* be changed from the current
 *	CPU.  If said PCU state is on the remote CPU, a cross-call will be
 *	sent by the owner LWP.  Therefore struct cpu_info::ci_pcu_curlwp[id]
 *	may only be changed by the current CPU and lwp_t::l_pcu_cpu[id] may
 *	only be cleared by the CPU which has the PCU state loaded.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_pcu.c,v 1.19 2014/05/25 14:53:55 rmind Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/lwp.h>
#include <sys/pcu.h>
#include <sys/ipi.h>

#if PCU_UNIT_COUNT > 0

static inline void pcu_do_op(const pcu_ops_t *, lwp_t * const, const int);
static void pcu_lwp_op(const pcu_ops_t *, lwp_t *, const int);

/*
 * Internal PCU commands for the pcu_do_op() function.
 */
#define	PCU_CMD_SAVE		0x01	/* save PCU state to the LWP */
#define	PCU_CMD_RELEASE		0x02	/* release PCU state on the CPU */

/*
 * Message structure for another CPU passed via ipi(9).
 */
typedef struct {
	const pcu_ops_t *pcu;
	lwp_t *		owner;
	const int	flags;
} pcu_ipi_msg_t;

/*
 * PCU IPIs run at IPL_HIGH (aka IPL_PCU in this code).
 */
#define	splpcu		splhigh

/* PCU operations structure provided by the MD code. */
extern const pcu_ops_t * const pcu_ops_md_defs[];

/*
 * pcu_switchpoint: release PCU state if the LWP is being run on another CPU.
 * This routine is called on each context switch by by mi_switch().
 */
void
pcu_switchpoint(lwp_t *l)
{
	const uint32_t pcu_valid = l->l_pcu_valid;
	int s;

	KASSERTMSG(l == curlwp, "l %p != curlwp %p", l, curlwp);

	if (__predict_true(pcu_valid == 0)) {
		/* PCUs are not in use. */
		return;
	}
	s = splpcu();
	for (u_int id = 0; id < PCU_UNIT_COUNT; id++) {
		if ((pcu_valid & (1U << id)) == 0) {
			continue;
		}
		struct cpu_info * const pcu_ci = l->l_pcu_cpu[id];
		if (pcu_ci == NULL || pcu_ci == l->l_cpu) {
			continue;
		}
		const pcu_ops_t * const pcu = pcu_ops_md_defs[id];
		pcu->pcu_state_release(l);
	}
	splx(s);
}

/*
 * pcu_discard_all: discard PCU state of the given LWP.
 *
 * Used by exec and LWP exit.
 */
void
pcu_discard_all(lwp_t *l)
{
	const uint32_t pcu_valid = l->l_pcu_valid;

	KASSERT(l == curlwp || ((l->l_flag & LW_SYSTEM) && pcu_valid == 0));

	if (__predict_true(pcu_valid == 0)) {
		/* PCUs are not in use. */
		return;
	}
	for (u_int id = 0; id < PCU_UNIT_COUNT; id++) {
		if ((pcu_valid & (1U << id)) == 0) {
			continue;
		}
		if (__predict_true(l->l_pcu_cpu[id] == NULL)) {
			continue;
		}
		const pcu_ops_t * const pcu = pcu_ops_md_defs[id];
		pcu_lwp_op(pcu, l, PCU_CMD_RELEASE);
	}
	l->l_pcu_valid = 0;
}

/*
 * pcu_save_all: save PCU state of the given LWP so that eg. coredump can
 * examine it.
 */
void
pcu_save_all(lwp_t *l)
{
	const uint32_t pcu_valid = l->l_pcu_valid;
	int flags = PCU_CMD_SAVE;

	/* If LW_WCORE, we are also releasing the state. */
	if (__predict_false(l->l_flag & LW_WCORE)) {
		flags |= PCU_CMD_RELEASE;
	}

	/*
	 * Normally we save for the current LWP, but sometimes we get called
	 * with a different LWP (forking a system LWP or doing a coredump of
	 * a process with multiple threads) and we need to deal with that.
	 */
	KASSERT(l == curlwp || (((l->l_flag & LW_SYSTEM) ||
	    (curlwp->l_proc == l->l_proc && l->l_stat == LSSUSPENDED)) &&
	    pcu_valid == 0));

	if (__predict_true(pcu_valid == 0)) {
		/* PCUs are not in use. */
		return;
	}
	for (u_int id = 0; id < PCU_UNIT_COUNT; id++) {
		if ((pcu_valid & (1U << id)) == 0) {
			continue;
		}
		if (__predict_true(l->l_pcu_cpu[id] == NULL)) {
			continue;
		}
		const pcu_ops_t * const pcu = pcu_ops_md_defs[id];
		pcu_lwp_op(pcu, l, flags);
	}
}

/*
 * pcu_do_op: save/release PCU state on the current CPU.
 *
 * => Must be called at IPL_PCU or from the interrupt.
 */
static inline void
pcu_do_op(const pcu_ops_t *pcu, lwp_t * const l, const int flags)
{
	struct cpu_info * const ci = curcpu();
	const u_int id = pcu->pcu_id;

	KASSERT(l->l_pcu_cpu[id] == ci);

	if (flags & PCU_CMD_SAVE) {
		pcu->pcu_state_save(l);
	}
	if (flags & PCU_CMD_RELEASE) {
		pcu->pcu_state_release(l);
		ci->ci_pcu_curlwp[id] = NULL;
		l->l_pcu_cpu[id] = NULL;
	}
}

/*
 * pcu_cpu_ipi: helper routine to call pcu_do_op() via ipi(9).
 */
static void
pcu_cpu_ipi(void *arg)
{
	const pcu_ipi_msg_t *pcu_msg = arg;
	const pcu_ops_t *pcu = pcu_msg->pcu;
	const u_int id = pcu->pcu_id;
	lwp_t *l = pcu_msg->owner;

	KASSERT(pcu_msg->owner != NULL);

	if (curcpu()->ci_pcu_curlwp[id] != l) {
		/*
		 * Different ownership: another LWP raced with us and
		 * perform save and release.  There is nothing to do.
		 */
		KASSERT(l->l_pcu_cpu[id] == NULL);
		return;
	}
	pcu_do_op(pcu, l, pcu_msg->flags);
}

/*
 * pcu_lwp_op: perform PCU state save, release or both operations on LWP.
 */
static void
pcu_lwp_op(const pcu_ops_t *pcu, lwp_t *l, const int flags)
{
	const u_int id = pcu->pcu_id;
	struct cpu_info *ci;
	int s;

	/*
	 * Caller should have re-checked if there is any state to manage.
	 * Block the interrupts and inspect again, since cross-call sent
	 * by remote CPU could have changed the state.
	 */
	s = splpcu();
	ci = l->l_pcu_cpu[id];
	if (ci == curcpu()) {
		/*
		 * State is on the current CPU - just perform the operations.
		 */
		KASSERTMSG(ci->ci_pcu_curlwp[id] == l,
		    "%s: cpu%u: pcu_curlwp[%u] (%p) != l (%p)",
		     __func__, cpu_index(ci), id, ci->ci_pcu_curlwp[id], l);
		pcu_do_op(pcu, l, flags);
		splx(s);
		return;
	}
	if (__predict_false(ci == NULL)) {
		/* Cross-call has won the race - no state to manage. */
		splx(s);
		return;
	}

	/*
	 * The state is on the remote CPU: perform the operation(s) there.
	 */
	pcu_ipi_msg_t pcu_msg = { .pcu = pcu, .owner = l, .flags = flags };
	ipi_msg_t ipi_msg = { .func = pcu_cpu_ipi, .arg = &pcu_msg };
	ipi_unicast(&ipi_msg, ci);
	splx(s);

	/* Wait for completion. */
	ipi_wait(&ipi_msg);

	KASSERT((flags & PCU_CMD_RELEASE) == 0 || l->l_pcu_cpu[id] == NULL);
}

/*
 * pcu_load: load/initialize the PCU state of current LWP on current CPU.
 */
void
pcu_load(const pcu_ops_t *pcu)
{
	lwp_t *oncpu_lwp, * const l = curlwp;
	const u_int id = pcu->pcu_id;
	struct cpu_info *ci, *curci;
	int s;

	KASSERT(!cpu_intr_p() && !cpu_softintr_p());

	s = splpcu();
	curci = curcpu();
	ci = l->l_pcu_cpu[id];

	/* Does this CPU already have our PCU state loaded? */
	if (ci == curci) {
		/*
		 * Fault reoccurred while the PCU state is loaded and
		 * therefore PCU should be re‐enabled.  This happens
		 * if LWP is context switched to another CPU and then
		 * switched back to the original CPU while the state
		 * on that CPU has not been changed by other LWPs.
		 *
		 * It may also happen due to instruction "bouncing" on
		 * some architectures.
		 */
		KASSERT(curci->ci_pcu_curlwp[id] == l);
		KASSERT(pcu_valid_p(pcu));
		pcu->pcu_state_load(l, PCU_VALID | PCU_REENABLE);
		splx(s);
		return;
	}

	/* If PCU state of this LWP is on the remote CPU - save it there. */
	if (ci) {
		pcu_ipi_msg_t pcu_msg = { .pcu = pcu, .owner = l,
		    .flags = PCU_CMD_SAVE | PCU_CMD_RELEASE };
		ipi_msg_t ipi_msg = { .func = pcu_cpu_ipi, .arg = &pcu_msg };
		ipi_unicast(&ipi_msg, ci);
		splx(s);

		/*
		 * Wait for completion, re-enter IPL_PCU and re-fetch
		 * the current CPU.
		 */
		ipi_wait(&ipi_msg);
		s = splpcu();
		curci = curcpu();
	}
	KASSERT(l->l_pcu_cpu[id] == NULL);

	/* Save the PCU state on the current CPU, if there is any. */
	if ((oncpu_lwp = curci->ci_pcu_curlwp[id]) != NULL) {
		pcu_do_op(pcu, oncpu_lwp, PCU_CMD_SAVE | PCU_CMD_RELEASE);
		KASSERT(curci->ci_pcu_curlwp[id] == NULL);
	}

	/*
	 * Finally, load the state for this LWP on this CPU.  Indicate to
	 * the load function whether PCU state was valid before this call.
	 */
	const bool valid = ((1U << id) & l->l_pcu_valid) != 0;
	pcu->pcu_state_load(l, valid ? PCU_VALID : 0);
	curci->ci_pcu_curlwp[id] = l;
	l->l_pcu_cpu[id] = curci;
	l->l_pcu_valid |= (1U << id);
	splx(s);
}

/*
 * pcu_discard: discard the PCU state of current LWP.  If "valid"
 * parameter is true, then keep considering the PCU state as valid.
 */
void
pcu_discard(const pcu_ops_t *pcu, bool valid)
{
	const u_int id = pcu->pcu_id;
	lwp_t * const l = curlwp;

	KASSERT(!cpu_intr_p() && !cpu_softintr_p());

	if (__predict_false(valid)) {
		l->l_pcu_valid |= (1U << id);
	} else {
		l->l_pcu_valid &= ~(1U << id);
	}
	if (__predict_true(l->l_pcu_cpu[id] == NULL)) {
		return;
	}
	pcu_lwp_op(pcu, l, PCU_CMD_RELEASE);
}

/*
 * pcu_save_lwp: save PCU state to the given LWP.
 */
void
pcu_save(const pcu_ops_t *pcu)
{
	const u_int id = pcu->pcu_id;
	lwp_t * const l = curlwp;

	KASSERT(!cpu_intr_p() && !cpu_softintr_p());

	if (__predict_true(l->l_pcu_cpu[id] == NULL)) {
		return;
	}
	pcu_lwp_op(pcu, l, PCU_CMD_SAVE | PCU_CMD_RELEASE);
}

/*
 * pcu_save_all_on_cpu: save all PCU states on the current CPU.
 */
void
pcu_save_all_on_cpu(void)
{
	int s;

	s = splpcu();
	for (u_int id = 0; id < PCU_UNIT_COUNT; id++) {
		const pcu_ops_t * const pcu = pcu_ops_md_defs[id];
		lwp_t *l;

		if ((l = curcpu()->ci_pcu_curlwp[id]) != NULL) {
			pcu_do_op(pcu, l, PCU_CMD_SAVE | PCU_CMD_RELEASE);
		}
	}
	splx(s);
}

/*
 * pcu_valid_p: return true if PCU state is considered valid.  Generally,
 * it always becomes "valid" when pcu_load() is called.
 */
bool
pcu_valid_p(const pcu_ops_t *pcu)
{
	const u_int id = pcu->pcu_id;
	lwp_t * const l = curlwp;

	return (l->l_pcu_valid & (1U << id)) != 0;
}

#endif /* PCU_UNIT_COUNT > 0 */
