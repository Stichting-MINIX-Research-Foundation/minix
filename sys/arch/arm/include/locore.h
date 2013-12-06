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

#ifndef _ARM_LOCORE_H_
#define _ARM_LOCORE_H_

#ifdef _KERNEL_OPT
#include "opt_cpuoptions.h"
#include "opt_cputypes.h"
#endif

#include <arm/cpuconf.h>
#include <arm/armreg.h>

#include <machine/frame.h>

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

#include <arm/cpufunc.h>

#ifdef __PROG32
#define IRQdisable __set_cpsr_c(I32_bit, I32_bit);
#define IRQenable __set_cpsr_c(I32_bit, 0);
#else
#define IRQdisable set_r15(R15_IRQ_DISABLE, R15_IRQ_DISABLE);
#define IRQenable set_r15(R15_IRQ_DISABLE, 0);
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
#endif /* __PROG32 */

#ifndef acorn26
/*
 * cpu device glue (belongs in cpuvar.h)
 */
void	cpu_attach(device_t, cpuid_t);
#endif

/* 1 == use cpu_sleep(), 0 == don't */
extern int cpu_do_powersave;
extern int cpu_fpu_present;
extern int cpu_hwdiv_present;

#if !defined(CPU_ARMV7)
#define	CPU_IS_ARMV7_P()		false
#elif defined(CPU_ARMV6) || defined(CPU_PRE_ARMV6)
extern bool cpu_armv7_p;
#define	CPU_IS_ARMV7_P()		(cpu_armv7_p)
#else
#define	CPU_IS_ARMV7_P()		true
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
void	userret(struct lwp *);

/* *_machdep.c */
void	bootsync(void);

/* fault.c */
int	badaddr_read(void *, size_t, void *);

/* syscall.c */
void	swi_handler(trapframe_t *);

/* arm_machdep.c */
void	ucas_ras_check(trapframe_t *);

/* vfp_init.c */
void	vfp_attach(void);
void	vfp_discardcontext(bool);
void	vfp_savecontext(void);
void	vfp_kernel_acquire(void);
void	vfp_kernel_release(void);
bool	vfp_used_p(void);
extern const pcu_ops_t arm_vfp_ops;

#endif	/* !_LOCORE */

#endif /* !_ARM_LOCORE_H_ */
