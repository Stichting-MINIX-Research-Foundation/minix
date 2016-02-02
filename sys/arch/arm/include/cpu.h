/*	cpu.h,v 1.45.4.7 2008/01/28 18:20:39 matt Exp	*/

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
 * cpu.h
 *
 * CPU specific symbols
 *
 * Created      : 18/09/94
 *
 * Based on kate/katelib/arm6.h
 */

#ifndef _ARM_CPU_H_
#define _ARM_CPU_H_

/*
 * User-visible definitions
 */

/*  CTL_MACHDEP definitions. */
#define	CPU_DEBUG		1	/* int: misc kernel debug control */
#define	CPU_BOOTED_DEVICE	2	/* string: device we booted from */
#define	CPU_BOOTED_KERNEL	3	/* string: kernel we booted */
#define	CPU_CONSDEV		4	/* struct: dev_t of our console */
#define	CPU_POWERSAVE		5	/* int: use CPU powersave mode */
#define	CPU_MAXID		6	/* number of valid machdep ids */

#if defined(_KERNEL) || defined(_KMEMUSER)

/*
 * Kernel-only definitions
 */

#if !defined(_MODULE) && defined(_KERNEL_OPT)
#include "opt_multiprocessor.h"
#include "opt_cpuoptions.h"
#include "opt_lockdebug.h"
#include "opt_cputypes.h"
#endif /* !_MODULE && _KERNEL_OPT */

#ifndef _LOCORE
#if defined(TPIDRPRW_IS_CURLWP) || defined(TPIDRPRW_IS_CURCPU)
#include <arm/armreg.h>
#endif

/* 1 == use cpu_sleep(), 0 == don't */
extern int cpu_do_powersave;
extern int cpu_fpu_present;

/* All the CLKF_* macros take a struct clockframe * as an argument. */

/*
 * CLKF_USERMODE: Return TRUE/FALSE (1/0) depending on whether the
 * frame came from USR mode or not.
 */
#ifdef __PROG32
#define CLKF_USERMODE(cf) (((cf)->cf_tf.tf_spsr & PSR_MODE) == PSR_USR32_MODE)
#else
#define CLKF_USERMODE(cf) (((cf)->cf_if.if_r15 & R15_MODE) == R15_MODE_USR)
#endif

/*
 * CLKF_INTR: True if we took the interrupt from inside another
 * interrupt handler.
 */
#if defined(__PROG32) && !defined(__ARM_EABI__)
/* Hack to treat FPE time as interrupt time so we can measure it */
#define CLKF_INTR(cf)						\
	((curcpu()->ci_intr_depth > 1) ||			\
	    ((cf)->cf_tf.tf_spsr & PSR_MODE) == PSR_UND32_MODE)
#else
#define CLKF_INTR(cf)	((void)(cf), curcpu()->ci_intr_depth > 1) 
#endif

/*
 * CLKF_PC: Extract the program counter from a clockframe
 */
#ifdef __PROG32
#define CLKF_PC(frame)		(frame->cf_tf.tf_pc)
#else
#define CLKF_PC(frame)		(frame->cf_if.if_r15 & R15_PC)
#endif

/*
 * LWP_PC: Find out the program counter for the given lwp.
 */
#ifdef __PROG32
#define LWP_PC(l)		(lwp_trapframe(l)->tf_pc)
#else
#define LWP_PC(l)		(lwp_trapframe(l)->tf_r15 & R15_PC)
#endif

/*
 * Per-CPU information.  For now we assume one CPU.
 */
#ifdef _KERNEL
static inline int curcpl(void);
static inline void set_curcpl(int);
static inline void cpu_dosoftints(void);
#endif

#ifdef _KMEMUSER
#include <sys/intr.h>
#endif
#include <sys/atomic.h>
#include <sys/cpu_data.h>
#include <sys/device_if.h>
#include <sys/evcnt.h>

struct cpu_info {
	struct cpu_data ci_data;	/* MI per-cpu data */
	device_t ci_dev;		/* Device corresponding to this CPU */
	cpuid_t ci_cpuid;
	uint32_t ci_arm_cpuid;		/* aggregate CPU id */
	uint32_t ci_arm_cputype;	/* CPU type */
	uint32_t ci_arm_cpurev;		/* CPU revision */
	uint32_t ci_ctrl;		/* The CPU control register */
	int ci_cpl;			/* current processor level (spl) */
	volatile int ci_astpending;	/* */
	int ci_want_resched;		/* resched() was called */
	int ci_intr_depth;		/* */
	struct cpu_softc *ci_softc;	/* platform softc */
	lwp_t *ci_softlwps[SOFTINT_COUNT];
	volatile uint32_t ci_softints;
	lwp_t *ci_curlwp;		/* current lwp */
	lwp_t *ci_lastlwp;		/* last lwp */
	struct evcnt ci_arm700bugcount;
	int32_t ci_mtx_count;
	int ci_mtx_oldspl;
	register_t ci_undefsave[3];
	uint32_t ci_vfp_id;
	uint64_t ci_lastintr;
	struct pmap_tlb_info *ci_tlb_info;
	struct pmap *ci_pmap_lastuser;
	struct pmap *ci_pmap_cur;
	tlb_asid_t ci_pmap_asid_cur;
	struct trapframe *ci_ddb_regs;
	struct evcnt ci_abt_evs[16];
	struct evcnt ci_und_ev;
	struct evcnt ci_und_cp15_ev;
	struct evcnt ci_vfp_evs[3];
#if defined(MP_CPU_INFO_MEMBERS)
	MP_CPU_INFO_MEMBERS
#endif
};

extern struct cpu_info cpu_info_store;

struct lwp *arm_curlwp(void);
struct cpu_info *arm_curcpu(void);

#if defined(_MODULE)

#define	curlwp		arm_curlwp()
#define curcpu()	arm_curcpu()

#elif defined(TPIDRPRW_IS_CURLWP)
static inline struct lwp *
_curlwp(void)
{
	return (struct lwp *) armreg_tpidrprw_read();
}

static inline void
_curlwp_set(struct lwp *l)
{
	armreg_tpidrprw_write((uintptr_t)l);
}

// Also in <sys/lwp.h> but also here if this was included before <sys/lwp.h> 
static inline struct cpu_info *lwp_getcpu(struct lwp *);

#define	curlwp		_curlwp()
// curcpu() expands into two instructions: a mrc and a ldr
#define	curcpu()	lwp_getcpu(_curlwp())
#elif defined(TPIDRPRW_IS_CURCPU)
#ifdef __HAVE_PREEMPTION
#error __HAVE_PREEMPTION requires TPIDRPRW_IS_CURLWP
#endif
static inline struct cpu_info *
curcpu(void)
{
	return (struct cpu_info *) armreg_tpidrprw_read();
}
#elif !defined(MULTIPROCESSOR)
#define	curcpu()	(&cpu_info_store)
#elif !defined(__HAVE_PREEMPTION)
#error MULTIPROCESSOR && !__HAVE_PREEMPTION requires TPIDRPRW_IS_CURCPU or TPIDRPRW_IS_CURLWP
#else
#error MULTIPROCESSOR && __HAVE_PREEMPTION requires TPIDRPRW_IS_CURLWP
#endif /* !TPIDRPRW_IS_CURCPU && !TPIDRPRW_IS_CURLWP */

#ifndef curlwp
#define	curlwp		(curcpu()->ci_curlwp)
#endif

#define CPU_INFO_ITERATOR	int
#if defined(MULTIPROCESSOR)
extern struct cpu_info *cpu_info[];
#define cpu_number()		(curcpu()->ci_index)
void cpu_boot_secondary_processors(void);
#define CPU_IS_PRIMARY(ci)	((ci)->ci_index == 0)
#define CPU_INFO_FOREACH(cii, ci)			\
	cii = 0, ci = cpu_info[0]; cii < ncpu && (ci = cpu_info[cii]) != NULL; cii++
#else 
#define cpu_number()            0

#define CPU_IS_PRIMARY(ci)	true
#define CPU_INFO_FOREACH(cii, ci)			\
	cii = 0, __USE(cii), ci = curcpu(); ci != NULL; ci = NULL
#endif

#define	LWP0_CPU_INFO	(&cpu_info_store)

static inline int
curcpl(void)
{
	return curcpu()->ci_cpl;
}

static inline void
set_curcpl(int pri)
{
	curcpu()->ci_cpl = pri;
}

static inline void
cpu_dosoftints(void)
{
#ifdef __HAVE_FAST_SOFTINTS
	void	dosoftints(void);
#ifndef __HAVE_PIC_FAST_SOFTINTS
	struct cpu_info * const ci = curcpu();
	if (ci->ci_intr_depth == 0 && (ci->ci_softints >> ci->ci_cpl) > 0)
		dosoftints();
#endif
#endif
}

#ifdef __PROG32
void	cpu_proc_fork(struct proc *, struct proc *);
#else
#define	cpu_proc_fork(p1, p2)
#endif

/*
 * Scheduling glue
 */

#ifdef __HAVE_PREEMPTION
#define setsoftast(ci)		atomic_or_uint(&(ci)->ci_astpending, __BIT(0))
#else
#define setsoftast(ci)		((ci)->ci_astpending = __BIT(0))
#endif

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */

#define cpu_signotify(l)		setsoftast((l)->l_cpu)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	cpu_need_proftick(l)	((l)->l_pflag |= LP_OWEUPC, \
				 setsoftast((l)->l_cpu))

/* for preeemption. */
void	cpu_set_curpri(int);

/*
 * We've already preallocated the stack for the idlelwps for additional CPUs.  
 * This hook allows to return them.
 */
vaddr_t cpu_uarea_alloc_idlelwp(struct cpu_info *);

#ifndef acorn26
/*
 * cpu device glue (belongs in cpuvar.h)
 */
void	cpu_attach(device_t, cpuid_t);
#endif

#endif /* !_LOCORE */

#endif /* _KERNEL */

#endif /* !_ARM_CPU_H_ */
