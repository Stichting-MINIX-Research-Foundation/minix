/*	cpufunc.h,v 1.40.22.4 2007/11/08 10:59:33 matt Exp	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
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
 * cpufunc.h
 *
 * Prototypes for cpu, mmu and tlb related functions.
 */

#ifndef _ARM_CPUFUNC_H_
#define _ARM_CPUFUNC_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <arm/armreg.h>
#include <arm/cpuconf.h>
#include <arm/armreg.h>
#include <arm/cpufunc_proto.h>

struct cpu_functions {

	/* CPU functions */

	u_int	(*cf_id)		(void);
	void	(*cf_cpwait)		(void);

	/* MMU functions */

	u_int	(*cf_control)		(u_int, u_int);
	void	(*cf_domains)		(u_int);
#if defined(ARM_MMU_EXTENDED)
	void	(*cf_setttb)		(u_int, tlb_asid_t);
#else
	void	(*cf_setttb)		(u_int, bool);
#endif
	u_int	(*cf_faultstatus)	(void);
	u_int	(*cf_faultaddress)	(void);

	/* TLB functions */

	void	(*cf_tlb_flushID)	(void);
	void	(*cf_tlb_flushID_SE)	(vaddr_t);
	void	(*cf_tlb_flushI)	(void);
	void	(*cf_tlb_flushI_SE)	(vaddr_t);
	void	(*cf_tlb_flushD)	(void);
	void	(*cf_tlb_flushD_SE)	(vaddr_t);

	/*
	 * Cache operations:
	 *
	 * We define the following primitives:
	 *
	 *	icache_sync_all		Synchronize I-cache
	 *	icache_sync_range	Synchronize I-cache range
	 *
	 *	dcache_wbinv_all	Write-back and Invalidate D-cache
	 *	dcache_wbinv_range	Write-back and Invalidate D-cache range
	 *	dcache_inv_range	Invalidate D-cache range
	 *	dcache_wb_range		Write-back D-cache range
	 *
	 *	idcache_wbinv_all	Write-back and Invalidate D-cache,
	 *				Invalidate I-cache
	 *	idcache_wbinv_range	Write-back and Invalidate D-cache,
	 *				Invalidate I-cache range
	 *
	 * Note that the ARM term for "write-back" is "clean".  We use
	 * the term "write-back" since it's a more common way to describe
	 * the operation.
	 *
	 * There are some rules that must be followed:
	 *
	 *	I-cache Synch (all or range):
	 *		The goal is to synchronize the instruction stream,
	 *		so you may beed to write-back dirty D-cache blocks
	 *		first.  If a range is requested, and you can't
	 *		synchronize just a range, you have to hit the whole
	 *		thing.
	 *
	 *	D-cache Write-Back and Invalidate range:
	 *		If you can't WB-Inv a range, you must WB-Inv the
	 *		entire D-cache.
	 *
	 *	D-cache Invalidate:
	 *		If you can't Inv the D-cache, you must Write-Back
	 *		and Invalidate.  Code that uses this operation
	 *		MUST NOT assume that the D-cache will not be written
	 *		back to memory.
	 *
	 *	D-cache Write-Back:
	 *		If you can't Write-back without doing an Inv,
	 *		that's fine.  Then treat this as a WB-Inv.
	 *		Skipping the invalidate is merely an optimization.
	 *
	 *	All operations:
	 *		Valid virtual addresses must be passed to each
	 *		cache operation.
	 */
	void	(*cf_icache_sync_all)	(void);
	void	(*cf_icache_sync_range)	(vaddr_t, vsize_t);

	void	(*cf_dcache_wbinv_all)	(void);
	void	(*cf_dcache_wbinv_range)(vaddr_t, vsize_t);
	void	(*cf_dcache_inv_range)	(vaddr_t, vsize_t);
	void	(*cf_dcache_wb_range)	(vaddr_t, vsize_t);

	void	(*cf_sdcache_wbinv_range)(vaddr_t, paddr_t, psize_t);
	void	(*cf_sdcache_inv_range)	(vaddr_t, paddr_t, psize_t);
	void	(*cf_sdcache_wb_range)	(vaddr_t, paddr_t, psize_t);

	void	(*cf_idcache_wbinv_all)	(void);
	void	(*cf_idcache_wbinv_range)(vaddr_t, vsize_t);

	/* Other functions */

	void	(*cf_flush_prefetchbuf)	(void);
	void	(*cf_drain_writebuf)	(void);
	void	(*cf_flush_brnchtgt_C)	(void);
	void	(*cf_flush_brnchtgt_E)	(u_int);

	void	(*cf_sleep)		(int mode);

	/* Soft functions */

	int	(*cf_dataabt_fixup)	(void *);
	int	(*cf_prefetchabt_fixup)	(void *);

#if defined(ARM_MMU_EXTENDED)
	void	(*cf_context_switch)	(u_int, tlb_asid_t);
#else
	void	(*cf_context_switch)	(u_int);
#endif

	void	(*cf_setup)		(char *);
};

extern struct cpu_functions cpufuncs;
extern u_int cputype;

#define cpu_id()		cpufuncs.cf_id()

#define cpu_control(c, e)	cpufuncs.cf_control(c, e)
#define cpu_domains(d)		cpufuncs.cf_domains(d)
#define cpu_setttb(t, f)	cpufuncs.cf_setttb(t, f)
#define cpu_faultstatus()	cpufuncs.cf_faultstatus()
#define cpu_faultaddress()	cpufuncs.cf_faultaddress()

#define	cpu_tlb_flushID()	cpufuncs.cf_tlb_flushID()
#define	cpu_tlb_flushID_SE(e)	cpufuncs.cf_tlb_flushID_SE(e)
#define	cpu_tlb_flushI()	cpufuncs.cf_tlb_flushI()
#define	cpu_tlb_flushI_SE(e)	cpufuncs.cf_tlb_flushI_SE(e)
#define	cpu_tlb_flushD()	cpufuncs.cf_tlb_flushD()
#define	cpu_tlb_flushD_SE(e)	cpufuncs.cf_tlb_flushD_SE(e)

#define	cpu_icache_sync_all()	cpufuncs.cf_icache_sync_all()
#define	cpu_icache_sync_range(a, s) cpufuncs.cf_icache_sync_range((a), (s))

#define	cpu_dcache_wbinv_all()	cpufuncs.cf_dcache_wbinv_all()
#define	cpu_dcache_wbinv_range(a, s) cpufuncs.cf_dcache_wbinv_range((a), (s))
#define	cpu_dcache_inv_range(a, s) cpufuncs.cf_dcache_inv_range((a), (s))
#define	cpu_dcache_wb_range(a, s) cpufuncs.cf_dcache_wb_range((a), (s))

#define	cpu_sdcache_wbinv_range(a, b, s) cpufuncs.cf_sdcache_wbinv_range((a), (b), (s))
#define	cpu_sdcache_inv_range(a, b, s) cpufuncs.cf_sdcache_inv_range((a), (b), (s))
#define	cpu_sdcache_wb_range(a, b, s) cpufuncs.cf_sdcache_wb_range((a), (b), (s))

#define	cpu_idcache_wbinv_all()	cpufuncs.cf_idcache_wbinv_all()
#define	cpu_idcache_wbinv_range(a, s) cpufuncs.cf_idcache_wbinv_range((a), (s))

#define	cpu_flush_prefetchbuf()	cpufuncs.cf_flush_prefetchbuf()
#define	cpu_drain_writebuf()	cpufuncs.cf_drain_writebuf()
#define	cpu_flush_brnchtgt_C()	cpufuncs.cf_flush_brnchtgt_C()
#define	cpu_flush_brnchtgt_E(e)	cpufuncs.cf_flush_brnchtgt_E(e)

#define cpu_sleep(m)		cpufuncs.cf_sleep(m)

#define cpu_dataabt_fixup(a)		cpufuncs.cf_dataabt_fixup(a)
#define cpu_prefetchabt_fixup(a)	cpufuncs.cf_prefetchabt_fixup(a)
#define ABORT_FIXUP_OK		0	/* fixup succeeded */
#define ABORT_FIXUP_FAILED	1	/* fixup failed */
#define ABORT_FIXUP_RETURN	2	/* abort handler should return */

#define cpu_context_switch(a)		cpufuncs.cf_context_switch(a)
#define cpu_setup(a)			cpufuncs.cf_setup(a)

int	set_cpufuncs		(void);
int	set_cpufuncs_id		(u_int);
#define ARCHITECTURE_NOT_PRESENT	1	/* known but not configured */
#define ARCHITECTURE_NOT_SUPPORTED	2	/* not known */

void	cpufunc_nullop		(void);
int	cpufunc_null_fixup	(void *);
int	early_abort_fixup	(void *);
int	late_abort_fixup	(void *);
u_int	cpufunc_id		(void);
u_int	cpufunc_control		(u_int, u_int);
void	cpufunc_domains		(u_int);
u_int	cpufunc_faultstatus	(void);
u_int	cpufunc_faultaddress	(void);

#define tlb_flush	cpu_tlb_flushID
#define setttb		cpu_setttb
#define drain_writebuf	cpu_drain_writebuf


#if defined(CPU_XSCALE)
#define	cpu_cpwait()		cpufuncs.cf_cpwait()
#endif

#ifndef cpu_cpwait
#define	cpu_cpwait()
#endif

/*
 * Macros for manipulating CPU interrupts
 */
#ifdef __PROG32
static __inline uint32_t __set_cpsr_c(uint32_t bic, uint32_t eor) __attribute__((__unused__));
static __inline uint32_t disable_interrupts(uint32_t mask) __attribute__((__unused__));
static __inline uint32_t enable_interrupts(uint32_t mask) __attribute__((__unused__));

static __inline uint32_t
__set_cpsr_c(uint32_t bic, uint32_t eor)
{
	uint32_t	tmp, ret;

	__asm volatile(
		"mrs     %0, cpsr\n"	/* Get the CPSR */
		"bic	 %1, %0, %2\n"	/* Clear bits */
		"eor	 %1, %1, %3\n"	/* XOR bits */
		"msr     cpsr_c, %1\n"	/* Set the control field of CPSR */
	: "=&r" (ret), "=&r" (tmp)
	: "r" (bic), "r" (eor) : "memory");

	return ret;
}

static __inline uint32_t
disable_interrupts(uint32_t mask)
{
	uint32_t	tmp, ret;
	mask &= (I32_bit | F32_bit);

	__asm volatile(
		"mrs     %0, cpsr\n"	/* Get the CPSR */
		"orr	 %1, %0, %2\n"	/* set bits */
		"msr     cpsr_c, %1\n"	/* Set the control field of CPSR */
	: "=&r" (ret), "=&r" (tmp)
	: "r" (mask)
	: "memory");

	return ret;
}

static __inline uint32_t
enable_interrupts(uint32_t mask)
{
	uint32_t	ret;
	mask &= (I32_bit | F32_bit);

	/* Get the CPSR */
	__asm __volatile("mrs\t%0, cpsr\n" : "=r"(ret));
#ifdef _ARM_ARCH_6
	if (__builtin_constant_p(mask)) {
		switch (mask) {
		case I32_bit | F32_bit:
			__asm __volatile("cpsie\tif");
			break;
		case I32_bit:
			__asm __volatile("cpsie\ti");
			break;
		case F32_bit:
			__asm __volatile("cpsie\tf");
			break;
		default:
			break;
		}
		return ret;
	}
#endif /* _ARM_ARCH_6 */

	/* Set the control field of CPSR */
	__asm volatile("msr\tcpsr_c, %0" :: "r"(ret & ~mask));

	return ret;
}

#define restore_interrupts(old_cpsr)					\
	(__set_cpsr_c((I32_bit | F32_bit), (old_cpsr) & (I32_bit | F32_bit)))

static inline void cpsie(register_t psw) __attribute__((__unused__));
static inline register_t cpsid(register_t psw) __attribute__((__unused__));

static inline void
cpsie(register_t psw)
{
#ifdef _ARM_ARCH_6
	if (!__builtin_constant_p(psw)) {
		enable_interrupts(psw);
		return;
	}
	switch (psw & (I32_bit|F32_bit)) {
	case I32_bit:		__asm("cpsie\ti"); break;
	case F32_bit:		__asm("cpsie\tf"); break;
	case I32_bit|F32_bit:	__asm("cpsie\tif"); break;
	}
#else
	enable_interrupts(psw);
#endif
}

static inline register_t
cpsid(register_t psw)
{
#ifdef _ARM_ARCH_6
	register_t oldpsw;
	if (!__builtin_constant_p(psw))
		return disable_interrupts(psw);

	__asm("mrs	%0, cpsr" : "=r"(oldpsw));
	switch (psw & (I32_bit|F32_bit)) {
	case I32_bit:		__asm("cpsid\ti"); break;
	case F32_bit:		__asm("cpsid\tf"); break;
	case I32_bit|F32_bit:	__asm("cpsid\tif"); break;
	}
	return oldpsw;
#else 
	return disable_interrupts(psw);
#endif
}

#else /* ! __PROG32 */
#define	disable_interrupts(mask)					\
	(set_r15((mask) & (R15_IRQ_DISABLE | R15_FIQ_DISABLE),		\
		 (mask) & (R15_IRQ_DISABLE | R15_FIQ_DISABLE)))

#define	enable_interrupts(mask)						\
	(set_r15((mask) & (R15_IRQ_DISABLE | R15_FIQ_DISABLE), 0))

#define	restore_interrupts(old_r15)					\
	(set_r15((R15_IRQ_DISABLE | R15_FIQ_DISABLE),			\
		 (old_r15) & (R15_IRQ_DISABLE | R15_FIQ_DISABLE)))
#endif /* __PROG32 */

#ifdef __PROG32
/* Functions to manipulate the CPSR. */
u_int	SetCPSR(u_int, u_int);
u_int	GetCPSR(void);
#else
/* Functions to manipulate the processor control bits in r15. */
u_int	set_r15(u_int, u_int);
u_int	get_r15(void);
#endif /* __PROG32 */


/*
 * CPU functions from locore.S
 */

void cpu_reset		(void) __dead;

/*
 * Cache info variables.
 */
#define	CACHE_TYPE_VIVT		0
#define	CACHE_TYPE_xxPT		1
#define	CACHE_TYPE_VIPT		1
#define	CACHE_TYPE_PIxx		2
#define	CACHE_TYPE_PIPT		3

/* PRIMARY CACHE VARIABLES */
struct arm_cache_info {
	u_int icache_size;
	u_int icache_line_size;
	u_int icache_ways;
	u_int icache_way_size;
	u_int icache_sets;

	u_int dcache_size;
	u_int dcache_line_size;
	u_int dcache_ways;
	u_int dcache_way_size;
	u_int dcache_sets;

	uint8_t cache_type;
	bool cache_unified;
	uint8_t icache_type;
	uint8_t dcache_type;
};

extern u_int arm_cache_prefer_mask;
extern u_int arm_dcache_align;
extern u_int arm_dcache_align_mask;

extern struct arm_cache_info arm_pcache;
extern struct arm_cache_info arm_scache;
#endif	/* _KERNEL */

#if defined(_KERNEL) || defined(_KMEMUSER)
/*
 * Miscellany
 */

int get_pc_str_offset	(void);

/*
 * Functions to manipulate cpu r13
 * (in arm/arm32/setstack.S)
 */

void set_stackptr	(u_int, u_int);
u_int get_stackptr	(u_int);

#endif /* _KERNEL || _KMEMUSER */

#endif	/* _ARM_CPUFUNC_H_ */

/* End of cpufunc.h */
