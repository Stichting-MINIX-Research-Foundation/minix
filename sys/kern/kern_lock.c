/*	$NetBSD: kern_lock.c,v 1.157 2015/04/11 15:24:25 skrll Exp $	*/

/*-
 * Copyright (c) 2002, 2006, 2007, 2008, 2009 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: kern_lock.c,v 1.157 2015/04/11 15:24:25 skrll Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lockdebug.h>
#include <sys/cpu.h>
#include <sys/syslog.h>
#include <sys/atomic.h>
#include <sys/lwp.h>

#include <machine/lock.h>

#include <dev/lockstat.h>

#define	RETURN_ADDRESS	(uintptr_t)__builtin_return_address(0)

bool	kernel_lock_dodebug;

__cpu_simple_lock_t kernel_lock[CACHE_LINE_SIZE / sizeof(__cpu_simple_lock_t)]
    __cacheline_aligned;

void
assert_sleepable(void)
{
	const char *reason;
	uint64_t pctr;
	bool idle;

	if (panicstr != NULL) {
		return;
	}

	LOCKDEBUG_BARRIER(kernel_lock, 1);

	/*
	 * Avoid disabling/re-enabling preemption here since this
	 * routine may be called in delicate situations.
	 */
	do {
		pctr = lwp_pctr();
		idle = CURCPU_IDLE_P();
	} while (pctr != lwp_pctr());

	reason = NULL;
	if (idle && !cold &&
	    kcpuset_isset(kcpuset_running, cpu_index(curcpu()))) {
		reason = "idle";
	}
	if (cpu_intr_p()) {
		reason = "interrupt";
	}
	if (cpu_softintr_p()) {
		reason = "softint";
	}

	if (reason) {
		panic("%s: %s caller=%p", __func__, reason,
		    (void *)RETURN_ADDRESS);
	}
}

/*
 * Functions for manipulating the kernel_lock.  We put them here
 * so that they show up in profiles.
 */

#define	_KERNEL_LOCK_ABORT(msg)						\
    LOCKDEBUG_ABORT(kernel_lock, &_kernel_lock_ops, __func__, msg)

#ifdef LOCKDEBUG
#define	_KERNEL_LOCK_ASSERT(cond)					\
do {									\
	if (!(cond))							\
		_KERNEL_LOCK_ABORT("assertion failed: " #cond);		\
} while (/* CONSTCOND */ 0)
#else
#define	_KERNEL_LOCK_ASSERT(cond)	/* nothing */
#endif

void	_kernel_lock_dump(volatile void *);

lockops_t _kernel_lock_ops = {
	"Kernel lock",
	LOCKOPS_SPIN,
	_kernel_lock_dump
};

/*
 * Initialize the kernel lock.
 */
void
kernel_lock_init(void)
{

	__cpu_simple_lock_init(kernel_lock);
	kernel_lock_dodebug = LOCKDEBUG_ALLOC(kernel_lock, &_kernel_lock_ops,
	    RETURN_ADDRESS);
}
CTASSERT(CACHE_LINE_SIZE >= sizeof(__cpu_simple_lock_t));

/*
 * Print debugging information about the kernel lock.
 */
void
_kernel_lock_dump(volatile void *junk)
{
	struct cpu_info *ci = curcpu();

	(void)junk;

	printf_nolog("curcpu holds : %18d wanted by: %#018lx\n",
	    ci->ci_biglock_count, (long)ci->ci_biglock_wanted);
}

/*
 * Acquire 'nlocks' holds on the kernel lock.
 */
void
_kernel_lock(int nlocks)
{
	struct cpu_info *ci;
	LOCKSTAT_TIMER(spintime);
	LOCKSTAT_FLAG(lsflag);
	struct lwp *owant;
	u_int spins;
	int s;
	struct lwp *l = curlwp;

	_KERNEL_LOCK_ASSERT(nlocks > 0);

	s = splvm();
	ci = curcpu();
	if (ci->ci_biglock_count != 0) {
		_KERNEL_LOCK_ASSERT(__SIMPLELOCK_LOCKED_P(kernel_lock));
		ci->ci_biglock_count += nlocks;
		l->l_blcnt += nlocks;
		splx(s);
		return;
	}

	_KERNEL_LOCK_ASSERT(l->l_blcnt == 0);
	LOCKDEBUG_WANTLOCK(kernel_lock_dodebug, kernel_lock, RETURN_ADDRESS,
	    0);

	if (__cpu_simple_lock_try(kernel_lock)) {
		ci->ci_biglock_count = nlocks;
		l->l_blcnt = nlocks;
		LOCKDEBUG_LOCKED(kernel_lock_dodebug, kernel_lock, NULL,
		    RETURN_ADDRESS, 0);
		splx(s);
		return;
	}

	/*
	 * To remove the ordering constraint between adaptive mutexes
	 * and kernel_lock we must make it appear as if this thread is
	 * blocking.  For non-interlocked mutex release, a store fence
	 * is required to ensure that the result of any mutex_exit()
	 * by the current LWP becomes visible on the bus before the set
	 * of ci->ci_biglock_wanted becomes visible.
	 */
	membar_producer();
	owant = ci->ci_biglock_wanted;
	ci->ci_biglock_wanted = l;

	/*
	 * Spin until we acquire the lock.  Once we have it, record the
	 * time spent with lockstat.
	 */
	LOCKSTAT_ENTER(lsflag);
	LOCKSTAT_START_TIMER(lsflag, spintime);

	spins = 0;
	do {
		splx(s);
		while (__SIMPLELOCK_LOCKED_P(kernel_lock)) {
			if (SPINLOCK_SPINOUT(spins)) {
				extern int start_init_exec;
				if (!start_init_exec)
					_KERNEL_LOCK_ABORT("spinout");
			}
			SPINLOCK_BACKOFF_HOOK;
			SPINLOCK_SPIN_HOOK;
		}
		s = splvm();
	} while (!__cpu_simple_lock_try(kernel_lock));

	ci->ci_biglock_count = nlocks;
	l->l_blcnt = nlocks;
	LOCKSTAT_STOP_TIMER(lsflag, spintime);
	LOCKDEBUG_LOCKED(kernel_lock_dodebug, kernel_lock, NULL,
	    RETURN_ADDRESS, 0);
	if (owant == NULL) {
		LOCKSTAT_EVENT_RA(lsflag, kernel_lock,
		    LB_KERNEL_LOCK | LB_SPIN, 1, spintime, RETURN_ADDRESS);
	}
	LOCKSTAT_EXIT(lsflag);
	splx(s);

	/*
	 * Now that we have kernel_lock, reset ci_biglock_wanted.  This
	 * store must be unbuffered (immediately visible on the bus) in
	 * order for non-interlocked mutex release to work correctly.
	 * It must be visible before a mutex_exit() can execute on this
	 * processor.
	 *
	 * Note: only where CAS is available in hardware will this be
	 * an unbuffered write, but non-interlocked release cannot be
	 * done on CPUs without CAS in hardware.
	 */
	(void)atomic_swap_ptr(&ci->ci_biglock_wanted, owant);

	/*
	 * Issue a memory barrier as we have acquired a lock.  This also
	 * prevents stores from a following mutex_exit() being reordered
	 * to occur before our store to ci_biglock_wanted above.
	 */
	membar_enter();
}

/*
 * Release 'nlocks' holds on the kernel lock.  If 'nlocks' is zero, release
 * all holds.
 */
void
_kernel_unlock(int nlocks, int *countp)
{
	struct cpu_info *ci;
	u_int olocks;
	int s;
	struct lwp *l = curlwp;

	_KERNEL_LOCK_ASSERT(nlocks < 2);

	olocks = l->l_blcnt;

	if (olocks == 0) {
		_KERNEL_LOCK_ASSERT(nlocks <= 0);
		if (countp != NULL)
			*countp = 0;
		return;
	}

	_KERNEL_LOCK_ASSERT(__SIMPLELOCK_LOCKED_P(kernel_lock));

	if (nlocks == 0)
		nlocks = olocks;
	else if (nlocks == -1) {
		nlocks = 1;
		_KERNEL_LOCK_ASSERT(olocks == 1);
	}
	s = splvm();
	ci = curcpu();
	_KERNEL_LOCK_ASSERT(ci->ci_biglock_count >= l->l_blcnt);
	if (ci->ci_biglock_count == nlocks) {
		LOCKDEBUG_UNLOCKED(kernel_lock_dodebug, kernel_lock,
		    RETURN_ADDRESS, 0);
		ci->ci_biglock_count = 0;
		__cpu_simple_unlock(kernel_lock);
		l->l_blcnt -= nlocks;
		splx(s);
		if (l->l_dopreempt)
			kpreempt(0);
	} else {
		ci->ci_biglock_count -= nlocks;
		l->l_blcnt -= nlocks;
		splx(s);
	}

	if (countp != NULL)
		*countp = olocks;
}

bool
_kernel_locked_p(void)
{
	return __SIMPLELOCK_LOCKED_P(kernel_lock);
}
