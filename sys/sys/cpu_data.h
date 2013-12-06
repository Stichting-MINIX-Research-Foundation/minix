/*	$NetBSD: cpu_data.h,v 1.36 2013/11/25 03:02:30 christos Exp $	*/

/*-
 * Copyright (c) 2004, 2006, 2007, 2008 The NetBSD Foundation, Inc.
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
 * based on arch/i386/include/cpu.h:
 *	NetBSD: cpu.h,v 1.115 2004/05/16 12:32:53 yamt Exp
 */

#ifndef _SYS_CPU_DATA_H_
#define	_SYS_CPU_DATA_H_

struct callout;
struct lwp;

#include <sys/sched.h>	/* for schedstate_percpu */
#include <sys/condvar.h>
#include <sys/pcu.h>
#include <sys/percpu_types.h>
#include <sys/queue.h>
#include <sys/kcpuset.h>

/*
 * MI per-cpu data
 *
 * this structure is intended to be included in MD cpu_info structure.
 *	struct cpu_info {
 *		struct cpu_data ci_data;
 *	}
 *
 * note that cpu_data is not expected to contain much data,
 * as cpu_info is size-limited on most ports.
 */

struct lockdebug;

struct cpu_data {
	/*
	 * The first section is likely to be touched by other CPUs -
	 * it is cache hot.
	 */
	lwp_t		*cpu_biglock_wanted;	/* LWP spinning on biglock */
	void		*cpu_callout;		/* per-CPU callout state */
	void		*cpu_unused1;		/* unused */
	u_int		cpu_unused2;		/* unused */
	struct schedstate_percpu cpu_schedstate; /* scheduler state */
	kcondvar_t	cpu_xcall;		/* cross-call support */
	int		cpu_xcall_pending;	/* cross-call support */
	lwp_t		*cpu_onproc;		/* bottom level LWP */

	cpuid_t		cpu_package_id;
	cpuid_t		cpu_core_id;
	cpuid_t		cpu_smt_id;

	struct lwp * volatile cpu_pcu_curlwp[PCU_UNIT_COUNT];

	/*
	 * This section is mostly CPU-private.
	 */
	lwp_t		*cpu_idlelwp;		/* idle lwp */
	void		*cpu_lockstat;		/* lockstat private tables */
	u_int		cpu_index;		/* CPU index */
	u_int		cpu_biglock_count;	/* # recursive holds */
	u_int		cpu_spin_locks;		/* # of spinlockmgr locks */
	u_int		cpu_simple_locks;	/* # of simple locks held */
	u_int		cpu_spin_locks2;	/* # of spin locks held XXX */
	u_int		cpu_lkdebug_recurse;	/* LOCKDEBUG recursion */
	u_int		cpu_softints;		/* pending (slow) softints */
	uint64_t	cpu_nsyscall;		/* syscall counter */
	uint64_t	cpu_ntrap;		/* trap counter */
	uint64_t	cpu_nswtch;		/* context switch counter */
	uint64_t	cpu_nintr;		/* interrupt count */
	uint64_t	cpu_nsoft;		/* soft interrupt count */
	uint64_t	cpu_nfault;		/* pagefault counter */
	void		*cpu_uvm;		/* uvm per-cpu data */
	void		*cpu_softcpu;		/* soft interrupt table */
	TAILQ_HEAD(,buf) cpu_biodone;		/* finished block xfers */
	percpu_cpu_t	cpu_percpu;		/* per-cpu data */
	struct selcluster *cpu_selcluster;	/* per-CPU select() info */
	void		*cpu_nch;		/* per-cpu vfs_cache data */
	_TAILQ_HEAD(,struct lockdebug,volatile) cpu_ld_locks;/* !: lockdebug */
	__cpu_simple_lock_t cpu_ld_lock;	/* lockdebug */
	uint64_t	cpu_cc_freq;		/* cycle counter frequency */
	int64_t		cpu_cc_skew;		/* counter skew vs cpu0 */
	char		cpu_name[8];		/* eg, "cpu4" */
	kcpuset_t	*cpu_kcpuset;		/* kcpuset_t of this cpu only */
};

/* compat definitions */
#define	ci_schedstate		ci_data.cpu_schedstate
#define	ci_index		ci_data.cpu_index
#define	ci_biglock_count	ci_data.cpu_biglock_count
#define	ci_biglock_wanted	ci_data.cpu_biglock_wanted
#define	ci_cpuname		ci_data.cpu_name
#define	ci_spin_locks		ci_data.cpu_spin_locks
#define	ci_simple_locks		ci_data.cpu_simple_locks
#define	ci_lockstat		ci_data.cpu_lockstat
#define	ci_spin_locks2		ci_data.cpu_spin_locks2
#define	ci_lkdebug_recurse	ci_data.cpu_lkdebug_recurse
#define	ci_pcu_curlwp		ci_data.cpu_pcu_curlwp
#define	ci_kcpuset		ci_data.cpu_kcpuset

#define	ci_package_id		ci_data.cpu_package_id
#define	ci_core_id		ci_data.cpu_core_id
#define	ci_smt_id		ci_data.cpu_smt_id

void	mi_cpu_init(void);
int	mi_cpu_attach(struct cpu_info *);

#endif /* _SYS_CPU_DATA_H_ */
