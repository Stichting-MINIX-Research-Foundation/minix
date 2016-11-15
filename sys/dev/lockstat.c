/*	$NetBSD: lockstat.c,v 1.24 2015/08/20 14:40:17 christos Exp $	*/

/*-
 * Copyright (c) 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Lock statistics driver, providing kernel support for the lockstat(8)
 * command.
 *
 * We use a global lock word (lockstat_lock) to track device opens.
 * Only one thread can hold the device at a time, providing a global lock.
 *
 * XXX Timings for contention on sleep locks are currently incorrect.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lockstat.c,v 1.24 2015/08/20 14:40:17 christos Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h> 
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/syslog.h>
#include <sys/atomic.h>

#include <dev/lockstat.h>

#include <machine/lock.h>

#include "ioconf.h"

#ifndef __HAVE_CPU_COUNTER
#error CPU counters not available
#endif

#if LONG_BIT == 64
#define	LOCKSTAT_HASH_SHIFT	3
#elif LONG_BIT == 32
#define	LOCKSTAT_HASH_SHIFT	2
#endif

#define	LOCKSTAT_MINBUFS	1000
#define	LOCKSTAT_DEFBUFS	10000
#define	LOCKSTAT_MAXBUFS	1000000

#define	LOCKSTAT_HASH_SIZE	128
#define	LOCKSTAT_HASH_MASK	(LOCKSTAT_HASH_SIZE - 1)
#define	LOCKSTAT_HASH(key)	\
	((key >> LOCKSTAT_HASH_SHIFT) & LOCKSTAT_HASH_MASK)

typedef struct lscpu {
	SLIST_HEAD(, lsbuf)	lc_free;
	u_int			lc_overflow;
	LIST_HEAD(lslist, lsbuf) lc_hash[LOCKSTAT_HASH_SIZE];
} lscpu_t;

typedef struct lslist lslist_t;

void	lockstat_start(lsenable_t *);
int	lockstat_alloc(lsenable_t *);
void	lockstat_init_tables(lsenable_t *);
int	lockstat_stop(lsdisable_t *);
void	lockstat_free(void);

dev_type_open(lockstat_open);
dev_type_close(lockstat_close);
dev_type_read(lockstat_read);
dev_type_ioctl(lockstat_ioctl);

volatile u_int	lockstat_enabled;
volatile u_int	lockstat_dev_enabled;
uintptr_t	lockstat_csstart;
uintptr_t	lockstat_csend;
uintptr_t	lockstat_csmask;
uintptr_t	lockstat_lamask;
uintptr_t	lockstat_lockstart;
uintptr_t	lockstat_lockend;
__cpu_simple_lock_t lockstat_lock;
lwp_t		*lockstat_lwp;
lsbuf_t		*lockstat_baseb;
size_t		lockstat_sizeb;
int		lockstat_busy;
struct timespec	lockstat_stime;

#ifdef KDTRACE_HOOKS
volatile u_int lockstat_dtrace_enabled;
CTASSERT(LB_NEVENT <= 3);
CTASSERT(LB_NLOCK <= (7 << LB_LOCK_SHIFT));
void
lockstat_probe_stub(uint32_t id, uintptr_t lock, uintptr_t callsite,
    uintptr_t flags, uintptr_t count, uintptr_t cycles)
{
}

uint32_t	lockstat_probemap[LS_NPROBES];
void		(*lockstat_probe_func)(uint32_t, uintptr_t, uintptr_t,
		    uintptr_t, uintptr_t, uintptr_t) = &lockstat_probe_stub;
#endif

const struct cdevsw lockstat_cdevsw = {
	.d_open = lockstat_open,
	.d_close = lockstat_close,
	.d_read = lockstat_read,
	.d_write = nowrite,
	.d_ioctl = lockstat_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

/*
 * Called when the pseudo-driver is attached.
 */
void
lockstatattach(int nunits)
{

	(void)nunits;

	__cpu_simple_lock_init(&lockstat_lock);
}

/*
 * Prepare the per-CPU tables for use, or clear down tables when tracing is
 * stopped.
 */
void
lockstat_init_tables(lsenable_t *le)
{
	int i, per, slop, cpuno;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	lscpu_t *lc;
	lsbuf_t *lb;

	/* coverity[assert_side_effect] */
	KASSERT(!lockstat_dev_enabled);

	for (CPU_INFO_FOREACH(cii, ci)) {
		if (ci->ci_lockstat != NULL) {
			kmem_free(ci->ci_lockstat, sizeof(lscpu_t));
			ci->ci_lockstat = NULL;
		}
	}

	if (le == NULL)
		return;

	lb = lockstat_baseb;
	per = le->le_nbufs / ncpu;
	slop = le->le_nbufs - (per * ncpu);
	cpuno = 0;
	for (CPU_INFO_FOREACH(cii, ci)) {
		lc = kmem_alloc(sizeof(*lc), KM_SLEEP);
		lc->lc_overflow = 0;
		ci->ci_lockstat = lc;

		SLIST_INIT(&lc->lc_free);
		for (i = 0; i < LOCKSTAT_HASH_SIZE; i++)
			LIST_INIT(&lc->lc_hash[i]);

		for (i = per; i != 0; i--, lb++) {
			lb->lb_cpu = (uint16_t)cpuno;
			SLIST_INSERT_HEAD(&lc->lc_free, lb, lb_chain.slist);
		}
		if (--slop > 0) {
			lb->lb_cpu = (uint16_t)cpuno;
			SLIST_INSERT_HEAD(&lc->lc_free, lb, lb_chain.slist);
			lb++;
		}
		cpuno++;
	}
}

/*
 * Start collecting lock statistics.
 */
void
lockstat_start(lsenable_t *le)
{

	/* coverity[assert_side_effect] */
	KASSERT(!lockstat_dev_enabled);

	lockstat_init_tables(le);

	if ((le->le_flags & LE_CALLSITE) != 0)
		lockstat_csmask = (uintptr_t)-1LL;
	else
		lockstat_csmask = 0;

	if ((le->le_flags & LE_LOCK) != 0)
		lockstat_lamask = (uintptr_t)-1LL;
	else
		lockstat_lamask = 0;

	lockstat_csstart = le->le_csstart;
	lockstat_csend = le->le_csend;
	lockstat_lockstart = le->le_lockstart;
	lockstat_lockstart = le->le_lockstart;
	lockstat_lockend = le->le_lockend;
	membar_sync();
	getnanotime(&lockstat_stime);
	lockstat_dev_enabled = le->le_mask;
	LOCKSTAT_ENABLED_UPDATE();
}

/*
 * Stop collecting lock statistics.
 */
int
lockstat_stop(lsdisable_t *ld)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	u_int cpuno, overflow;
	struct timespec ts;
	int error;
	lwp_t *l;

	/* coverity[assert_side_effect] */
	KASSERT(lockstat_dev_enabled);

	/*
	 * Set enabled false, force a write barrier, and wait for other CPUs
	 * to exit lockstat_event().
	 */
	lockstat_dev_enabled = 0;
	LOCKSTAT_ENABLED_UPDATE();
	getnanotime(&ts);
	tsleep(&lockstat_stop, PPAUSE, "lockstat", mstohz(10));

	/*
	 * Did we run out of buffers while tracing?
	 */
	overflow = 0;
	for (CPU_INFO_FOREACH(cii, ci))
		overflow += ((lscpu_t *)ci->ci_lockstat)->lc_overflow;

	if (overflow != 0) {
		error = EOVERFLOW;
		log(LOG_NOTICE, "lockstat: %d buffer allocations failed\n",
		    overflow);
	} else
		error = 0;

	lockstat_init_tables(NULL);

	/* Run through all LWPs and clear the slate for the next run. */
	mutex_enter(proc_lock);
	LIST_FOREACH(l, &alllwp, l_list) {
		l->l_pfailaddr = 0;
		l->l_pfailtime = 0;
		l->l_pfaillock = 0;
	}
	mutex_exit(proc_lock);

	if (ld == NULL)
		return error;

	/*
	 * Fill out the disable struct for the caller.
	 */
	timespecsub(&ts, &lockstat_stime, &ld->ld_time);
	ld->ld_size = lockstat_sizeb;

	cpuno = 0;
	for (CPU_INFO_FOREACH(cii, ci)) {
		if (cpuno >= sizeof(ld->ld_freq) / sizeof(ld->ld_freq[0])) {
			log(LOG_WARNING, "lockstat: too many CPUs\n");
			break;
		}
		ld->ld_freq[cpuno++] = cpu_frequency(ci);
	}

	return error;
}

/*
 * Allocate buffers for lockstat_start().
 */
int
lockstat_alloc(lsenable_t *le)
{
	lsbuf_t *lb;
	size_t sz;

	/* coverity[assert_side_effect] */
	KASSERT(!lockstat_dev_enabled);
	lockstat_free();

	sz = sizeof(*lb) * le->le_nbufs;

	lb = kmem_zalloc(sz, KM_SLEEP);
	if (lb == NULL)
		return (ENOMEM);

	/* coverity[assert_side_effect] */
	KASSERT(!lockstat_dev_enabled);
	KASSERT(lockstat_baseb == NULL);
	lockstat_sizeb = sz;
	lockstat_baseb = lb;
		
	return (0);
}

/*
 * Free allocated buffers after tracing has stopped.
 */
void
lockstat_free(void)
{

	/* coverity[assert_side_effect] */
	KASSERT(!lockstat_dev_enabled);

	if (lockstat_baseb != NULL) {
		kmem_free(lockstat_baseb, lockstat_sizeb);
		lockstat_baseb = NULL;
	}
}

/*
 * Main entry point from lock primatives.
 */
void
lockstat_event(uintptr_t lock, uintptr_t callsite, u_int flags, u_int count,
	       uint64_t cycles)
{
	lslist_t *ll;
	lscpu_t *lc;
	lsbuf_t *lb;
	u_int event;
	int s;

#ifdef KDTRACE_HOOKS
	uint32_t id;
	CTASSERT((LS_NPROBES & (LS_NPROBES - 1)) == 0);
	if ((id = lockstat_probemap[LS_COMPRESS(flags)]) != 0)
		(*lockstat_probe_func)(id, lock, callsite, flags, count,
		    cycles);
#endif

	if ((flags & lockstat_dev_enabled) != flags || count == 0)
		return;
	if (lock < lockstat_lockstart || lock > lockstat_lockend)
		return;
	if (callsite < lockstat_csstart || callsite > lockstat_csend)
		return;

	callsite &= lockstat_csmask;
	lock &= lockstat_lamask;

	/*
	 * Find the table for this lock+callsite pair, and try to locate a
	 * buffer with the same key.
	 */
	s = splhigh();
	lc = curcpu()->ci_lockstat;
	ll = &lc->lc_hash[LOCKSTAT_HASH(lock ^ callsite)];
	event = (flags & LB_EVENT_MASK) - 1;

	LIST_FOREACH(lb, ll, lb_chain.list) {
		if (lb->lb_lock == lock && lb->lb_callsite == callsite)
			break;
	}

	if (lb != NULL) {
		/*
		 * We found a record.  Move it to the front of the list, as
		 * we're likely to hit it again soon.
		 */
		if (lb != LIST_FIRST(ll)) {
			LIST_REMOVE(lb, lb_chain.list);
			LIST_INSERT_HEAD(ll, lb, lb_chain.list);
		}
		lb->lb_counts[event] += count;
		lb->lb_times[event] += cycles;
	} else if ((lb = SLIST_FIRST(&lc->lc_free)) != NULL) {
		/*
		 * Pinch a new buffer and fill it out.
		 */
		SLIST_REMOVE_HEAD(&lc->lc_free, lb_chain.slist);
		LIST_INSERT_HEAD(ll, lb, lb_chain.list);
		lb->lb_flags = (uint16_t)flags;
		lb->lb_lock = lock;
		lb->lb_callsite = callsite;
		lb->lb_counts[event] = count;
		lb->lb_times[event] = cycles;
	} else {
		/*
		 * We didn't find a buffer and there were none free.
		 * lockstat_stop() will notice later on and report the
		 * error.
		 */
		 lc->lc_overflow++;
	}

	splx(s);
}

/*
 * Accept an open() on /dev/lockstat.
 */
int
lockstat_open(dev_t dev, int flag, int mode, lwp_t *l)
{

	if (!__cpu_simple_lock_try(&lockstat_lock))
		return EBUSY;
	lockstat_lwp = curlwp;
	return 0;
}

/*
 * Accept the last close() on /dev/lockstat.
 */
int
lockstat_close(dev_t dev, int flag, int mode, lwp_t *l)
{

	lockstat_lwp = NULL;
	__cpu_simple_unlock(&lockstat_lock);
	return 0;
}

/*
 * Handle control operations.
 */
int
lockstat_ioctl(dev_t dev, u_long cmd, void *data, int flag, lwp_t *l)
{
	lsenable_t *le;
	int error;

	if (lockstat_lwp != curlwp)
		return EBUSY;

	switch (cmd) {
	case IOC_LOCKSTAT_GVERSION:
		*(int *)data = LS_VERSION;
		error = 0;
		break;

	case IOC_LOCKSTAT_ENABLE:
		le = (lsenable_t *)data;

		if (!cpu_hascounter()) {
			error = ENODEV;
			break;
		}
		if (lockstat_dev_enabled) {
			error = EBUSY;
			break;
		}

		/*
		 * Sanitize the arguments passed in and set up filtering.
		 */
		if (le->le_nbufs == 0)
			le->le_nbufs = LOCKSTAT_DEFBUFS;
		else if (le->le_nbufs > LOCKSTAT_MAXBUFS ||
		    le->le_nbufs < LOCKSTAT_MINBUFS) {
			error = EINVAL;
			break;
		}
		if ((le->le_flags & LE_ONE_CALLSITE) == 0) {
			le->le_csstart = 0;
			le->le_csend = le->le_csstart - 1;
		}
		if ((le->le_flags & LE_ONE_LOCK) == 0) {
			le->le_lockstart = 0;
			le->le_lockend = le->le_lockstart - 1;
		}
		if ((le->le_mask & LB_EVENT_MASK) == 0)
			return EINVAL;
		if ((le->le_mask & LB_LOCK_MASK) == 0)
			return EINVAL;

		/*
		 * Start tracing.
		 */
		if ((error = lockstat_alloc(le)) == 0)
			lockstat_start(le);
		break;

	case IOC_LOCKSTAT_DISABLE:
		if (!lockstat_dev_enabled)
			error = EINVAL;
		else
			error = lockstat_stop((lsdisable_t *)data);
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}

/*
 * Copy buffers out to user-space.
 */
int
lockstat_read(dev_t dev, struct uio *uio, int flag)
{

	if (curlwp != lockstat_lwp || lockstat_dev_enabled)
		return EBUSY;
	return uiomove(lockstat_baseb, lockstat_sizeb, uio);
}
