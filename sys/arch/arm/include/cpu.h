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

#if !defined(_LKM) && defined(_KERNEL_OPT)
#include "opt_multiprocessor.h"
#include "opt_cpuoptions.h"
#include "opt_lockdebug.h"
#include "opt_cputypes.h"
#endif /* !_LKM && _KERNEL_OPT */

#include <arm/cpuconf.h>

#ifndef _LOCORE
#include <machine/frame.h>
#endif	/* !_LOCORE */

#include <arm/armreg.h>


#ifndef _LOCORE
/* 1 == use cpu_sleep(), 0 == don't */
extern int cpu_do_powersave;
#endif

#ifdef _LOCORE

#if defined(_ARM_ARCH_6)
#define IRQdisable	cpsid	i
#define IRQenable	cpsie	i
#elif defined(__PROG32)
#define IRQdisable \
	stmfd	sp!, {r0} ; \
	mrs	r0, cpsr ; \
	orr	r0, r0, #(I32_bit) ; \
	msr	cpsr_c, r0 ; \
	ldmfd	sp!, {r0}

#define IRQenable \
	stmfd	sp!, {r0} ; \
	mrs	r0, cpsr ; \
	bic	r0, r0, #(I32_bit) ; \
	msr	cpsr_c, r0 ; \
	ldmfd	sp!, {r0}		
#else
/* Not yet used in 26-bit code */
#endif

#if defined (TPIDRPRW_IS_CURCPU)
#define GET_CURCPU(rX)		mrc	p15, 0, rX, c13, c0, 4
#define GET_CURLWP(rX)		GET_CURCPU(rX); ldr rX, [rX, #CI_CURLWP]
#elif defined (TPIDRPRW_IS_CURLWP)
#define GET_CURLWP(rX)		mrc	p15, 0, rX, c13, c0, 4
#define GET_CURCPU(rX)		GET_CURLWP(rX); ldr rX, [rX, #L_CPU]
#elif !defined(MULTIPROCESSOR)
#define GET_CURCPU(rX)		ldr rX, =_C_LABEL(cpu_info_store)
#define GET_CURLWP(rX)		GET_CURCPU(rX); ldr rX, [rX, #CI_CURLWP]
#endif
#define GET_CURPCB(rX)		GET_CURLWP(rX); ldr rX, [rX, #L_PCB]

#else /* !_LOCORE */

#ifdef __PROG32
#define IRQdisable __set_cpsr_c(I32_bit, I32_bit);
#define IRQenable __set_cpsr_c(I32_bit, 0);
#else
#define IRQdisable set_r15(R15_IRQ_DISABLE, R15_IRQ_DISABLE);
#define IRQenable set_r15(R15_IRQ_DISABLE, 0);
#endif

#endif /* !_LOCORE */

#ifndef _LOCORE

/* All the CLKF_* macros take a struct clockframe * as an argument. */

/*
 * CLKF_USERMODE: Return TRUE/FALSE (1/0) depending on whether the
 * frame came from USR mode or not.
 */
#ifdef __PROG32
#define CLKF_USERMODE(frame)	((frame->cf_tf.tf_spsr & PSR_MODE) == PSR_USR32_MODE)
#else
#define CLKF_USERMODE(frame)	((frame->cf_if.if_r15 & R15_MODE) == R15_MODE_USR)
#endif

/*
 * CLKF_INTR: True if we took the interrupt from inside another
 * interrupt handler.
 */
#ifdef __PROG32
/* Hack to treat FPE time as interrupt time so we can measure it */
#define CLKF_INTR(frame)						\
	((curcpu()->ci_intr_depth > 1) ||				\
	    (frame->cf_tf.tf_spsr & PSR_MODE) == PSR_UND32_MODE)
#else
#define CLKF_INTR(frame)	(curcpu()->ci_intr_depth > 1) 
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
 * Validate a PC or PSR for a user process.  Used by various system calls
 * that take a context passed by the user and restore it.
 */

#ifdef __PROG32
#define VALID_R15_PSR(r15,psr)						\
	(((psr) & PSR_MODE) == PSR_USR32_MODE &&			\
		((psr) & (I32_bit | F32_bit)) == 0)
#else
#define VALID_R15_PSR(r15,psr)						\
	(((r15) & R15_MODE) == R15_MODE_USR &&				\
		((r15) & (R15_IRQ_DISABLE | R15_FIQ_DISABLE)) == 0)
#endif



/* The address of the vector page. */
extern vaddr_t vector_page;
#ifdef __PROG32
void	arm32_vector_init(vaddr_t, int);

#define	ARM_VEC_RESET			(1 << 0)
#define	ARM_VEC_UNDEFINED		(1 << 1)
#define	ARM_VEC_SWI			(1 << 2)
#define	ARM_VEC_PREFETCH_ABORT		(1 << 3)
#define	ARM_VEC_DATA_ABORT		(1 << 4)
#define	ARM_VEC_ADDRESS_EXCEPTION	(1 << 5)
#define	ARM_VEC_IRQ			(1 << 6)
#define	ARM_VEC_FIQ			(1 << 7)

#define	ARM_NVEC			8
#define	ARM_VEC_ALL			0xffffffff
#endif

/*
 * Per-CPU information.  For now we assume one CPU.
 */
static inline int curcpl(void);
static inline void set_curcpl(int);
static inline void cpu_dosoftints(void);

#include <sys/device_if.h>
#include <sys/evcnt.h>
#include <sys/cpu_data.h>

struct cpu_info {
	struct cpu_data ci_data;	/* MI per-cpu data */
	device_t ci_dev;		/* Device corresponding to this CPU */
	cpuid_t ci_cpuid;
	uint32_t ci_arm_cpuid;		/* aggregate CPU id */
	uint32_t ci_arm_cputype;	/* CPU type */
	uint32_t ci_arm_cpurev;		/* CPU revision */
	uint32_t ci_ctrl;		/* The CPU control register */
	int ci_cpl;			/* current processor level (spl) */
	int ci_astpending;		/* */
	int ci_want_resched;		/* resched() was called */
	int ci_intr_depth;		/* */
	struct cpu_softc *ci_softc;	/* platform softc */
#ifdef __HAVE_FAST_SOFTINTS
	lwp_t *ci_softlwps[SOFTINT_COUNT];
	volatile uint32_t ci_softints;
#endif
	lwp_t *ci_curlwp;		/* current lwp */
	struct evcnt ci_arm700bugcount;
	int32_t ci_mtx_count;
	int ci_mtx_oldspl;
	register_t ci_undefsave[3];
	uint32_t ci_vfp_id;
#if defined(_ARM_ARCH_7)
	uint64_t ci_lastintr;
#endif
	struct evcnt ci_abt_evs[FAULT_TYPE_MASK+1];
#if defined(MP_CPU_INFO_MEMBERS)
	MP_CPU_INFO_MEMBERS
#endif
};

extern struct cpu_info cpu_info_store;
#if defined(TPIDRPRW_IS_CURLWP)
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

#define	curlwp		(_curlwp())
static inline struct cpu_info *
curcpu(void)
{
	return curlwp->l_cpu;
}
#elif defined(TPIDRPRW_IS_CURCPU)
static inline struct cpu_info *
curcpu(void)
{
	return (struct cpu_info *) armreg_tpidrprw_read();
}
#elif !defined(MULTIPROCESSOR)
#define	curcpu()	(&cpu_info_store)
#else
#error MULTIPROCESSOR requires TPIDRPRW_IS_CURLWP or TPIDRPRW_IS_CURCPU
#endif /* !TPIDRPRW_IS_CURCPU && !TPIDRPRW_IS_CURLWP */

#ifndef curlwp
#define	curlwp		(curcpu()->ci_curlwp)
#endif

#define CPU_INFO_ITERATOR	int
#if defined(MULTIPROCESSOR)
extern struct cpu_info *cpu_info[];
#define cpu_number()	(curcpu()->ci_cpuid)
void cpu_boot_secondary_processors(void);
#define CPU_IS_PRIMARY(ci)	((ci)->ci_cpuid == 0)
#define CPU_INFO_FOREACH(cii, ci)			\
	cii = 0, ci = cpu_info[0]; cii < ncpu && (ci = cpu_info[cii]) != NULL; cii++
#else 
#define cpu_number()            0

#define CPU_IS_PRIMARY(ci)	true
#define CPU_INFO_FOREACH(cii, ci)			\
	cii = 0, ci = curcpu(); ci != NULL; ci = NULL
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

#define setsoftast()			(curcpu()->ci_astpending = 1)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */

#define cpu_signotify(l)		setsoftast()

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	cpu_need_proftick(l)	((l)->l_pflag |= LP_OWEUPC, setsoftast())

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

/*
 * Random cruft
 */

struct lwp;

/* locore.S */
void atomic_set_bit(u_int *, u_int);
void atomic_clear_bit(u_int *, u_int);

/* cpuswitch.S */
struct pcb;
void	savectx(struct pcb *);

/* ast.c */
void userret(register struct lwp *);

/* *_machdep.c */
void bootsync(void);

/* fault.c */
int badaddr_read(void *, size_t, void *);

/* syscall.c */
void swi_handler(trapframe_t *);

/* arm_machdep.c */
void ucas_ras_check(trapframe_t *);

/* vfp_init.c */
void vfp_attach(void);
void vfp_discardcontext(void);
void vfp_savecontext(void);
extern const pcu_ops_t arm_vfp_ops;

#endif	/* !_LOCORE */

#endif /* _KERNEL */

#endif /* !_ARM_CPU_H_ */
