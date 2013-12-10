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

#ifndef _ARM32_CPUFUNC_H_
#define _ARM32_CPUFUNC_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <arm/armreg.h>
#include <arm/cpuconf.h>
#include <arm/armreg.h>

struct cpu_functions {

	/* CPU functions */

	u_int	(*cf_id)		(void);
	void	(*cf_cpwait)		(void);

	/* MMU functions */

	u_int	(*cf_control)		(u_int, u_int);
	void	(*cf_domains)		(u_int);
	void	(*cf_setttb)		(u_int, bool);
	u_int	(*cf_faultstatus)	(void);
	u_int	(*cf_faultaddress)	(void);

	/* TLB functions */

	void	(*cf_tlb_flushID)	(void);
	void	(*cf_tlb_flushID_SE)	(u_int);
	void	(*cf_tlb_flushI)	(void);
	void	(*cf_tlb_flushI_SE)	(u_int);
	void	(*cf_tlb_flushD)	(void);
	void	(*cf_tlb_flushD_SE)	(u_int);

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

	void	(*cf_context_switch)	(u_int);

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

#if defined(CPU_ARM2) || defined(CPU_ARM250) || defined(CPU_ARM3)
void	arm3_cache_flush	(void);
#endif	/* CPU_ARM2 || CPU_ARM250 || CPU_ARM3 */

#ifdef CPU_ARM2
u_int	arm2_id			(void);
#endif /* CPU_ARM2 */

#ifdef CPU_ARM250
u_int	arm250_id		(void);
#endif

#ifdef CPU_ARM3
u_int	arm3_control		(u_int, u_int);
#endif	/* CPU_ARM3 */

#if defined(CPU_ARM6) || defined(CPU_ARM7)
void	arm67_setttb		(u_int, bool);
void	arm67_tlb_flush		(void);
void	arm67_tlb_purge		(u_int);
void	arm67_cache_flush	(void);
void	arm67_context_switch	(u_int);
#endif	/* CPU_ARM6 || CPU_ARM7 */

#ifdef CPU_ARM6
void	arm6_setup		(char *);
#endif	/* CPU_ARM6 */

#ifdef CPU_ARM7
void	arm7_setup		(char *);
#endif	/* CPU_ARM7 */

#ifdef CPU_ARM7TDMI
int	arm7_dataabt_fixup	(void *);
void	arm7tdmi_setup		(char *);
void	arm7tdmi_setttb		(u_int, bool);
void	arm7tdmi_tlb_flushID	(void);
void	arm7tdmi_tlb_flushID_SE	(u_int);
void	arm7tdmi_cache_flushID	(void);
void	arm7tdmi_context_switch	(u_int);
#endif /* CPU_ARM7TDMI */

#ifdef CPU_ARM8
void	arm8_setttb		(u_int, bool);
void	arm8_tlb_flushID	(void);
void	arm8_tlb_flushID_SE	(u_int);
void	arm8_cache_flushID	(void);
void	arm8_cache_flushID_E	(u_int);
void	arm8_cache_cleanID	(void);
void	arm8_cache_cleanID_E	(u_int);
void	arm8_cache_purgeID	(void);
void	arm8_cache_purgeID_E	(u_int entry);

void	arm8_cache_syncI	(void);
void	arm8_cache_cleanID_rng	(vaddr_t, vsize_t);
void	arm8_cache_cleanD_rng	(vaddr_t, vsize_t);
void	arm8_cache_purgeID_rng	(vaddr_t, vsize_t);
void	arm8_cache_purgeD_rng	(vaddr_t, vsize_t);
void	arm8_cache_syncI_rng	(vaddr_t, vsize_t);

void	arm8_context_switch	(u_int);

void	arm8_setup		(char *);

u_int	arm8_clock_config	(u_int, u_int);
#endif

#ifdef CPU_FA526
void	fa526_setup		(char *);
void	fa526_setttb		(u_int, bool);
void	fa526_context_switch	(u_int);
void	fa526_cpu_sleep		(int);
void	fa526_tlb_flushI_SE	(u_int);
void	fa526_tlb_flushID_SE	(u_int);
void	fa526_flush_prefetchbuf	(void);
void	fa526_flush_brnchtgt_E	(u_int);

void	fa526_icache_sync_all	(void);
void	fa526_icache_sync_range(vaddr_t, vsize_t);
void	fa526_dcache_wbinv_all	(void);
void	fa526_dcache_wbinv_range(vaddr_t, vsize_t);
void	fa526_dcache_inv_range	(vaddr_t, vsize_t);
void	fa526_dcache_wb_range	(vaddr_t, vsize_t);
void	fa526_idcache_wbinv_all(void);
void	fa526_idcache_wbinv_range(vaddr_t, vsize_t);
#endif

#ifdef CPU_SA110
void	sa110_setup		(char *);
void	sa110_context_switch	(u_int);
#endif	/* CPU_SA110 */

#if defined(CPU_SA1100) || defined(CPU_SA1110)
void	sa11x0_drain_readbuf	(void);

void	sa11x0_context_switch	(u_int);
void	sa11x0_cpu_sleep	(int);

void	sa11x0_setup		(char *);
#endif

#if defined(CPU_SA110) || defined(CPU_SA1100) || defined(CPU_SA1110)
void	sa1_setttb		(u_int, bool);

void	sa1_tlb_flushID_SE	(u_int);

void	sa1_cache_flushID	(void);
void	sa1_cache_flushI	(void);
void	sa1_cache_flushD	(void);
void	sa1_cache_flushD_SE	(u_int);

void	sa1_cache_cleanID	(void);
void	sa1_cache_cleanD	(void);
void	sa1_cache_cleanD_E	(u_int);

void	sa1_cache_purgeID	(void);
void	sa1_cache_purgeID_E	(u_int);
void	sa1_cache_purgeD	(void);
void	sa1_cache_purgeD_E	(u_int);

void	sa1_cache_syncI		(void);
void	sa1_cache_cleanID_rng	(vaddr_t, vsize_t);
void	sa1_cache_cleanD_rng	(vaddr_t, vsize_t);
void	sa1_cache_purgeID_rng	(vaddr_t, vsize_t);
void	sa1_cache_purgeD_rng	(vaddr_t, vsize_t);
void	sa1_cache_syncI_rng	(vaddr_t, vsize_t);

#endif

#ifdef CPU_ARM9
void	arm9_setttb		(u_int, bool);

void	arm9_tlb_flushID_SE	(u_int);

void	arm9_icache_sync_all	(void);
void	arm9_icache_sync_range	(vaddr_t, vsize_t);

void	arm9_dcache_wbinv_all	(void);
void	arm9_dcache_wbinv_range (vaddr_t, vsize_t);
void	arm9_dcache_inv_range	(vaddr_t, vsize_t);
void	arm9_dcache_wb_range	(vaddr_t, vsize_t);

void	arm9_idcache_wbinv_all	(void);
void	arm9_idcache_wbinv_range (vaddr_t, vsize_t);

void	arm9_context_switch	(u_int);

void	arm9_setup		(char *);

extern unsigned arm9_dcache_sets_max;
extern unsigned arm9_dcache_sets_inc;
extern unsigned arm9_dcache_index_max;
extern unsigned arm9_dcache_index_inc;
#endif

#if defined(CPU_ARM9E) || defined(CPU_ARM10) || defined(CPU_SHEEVA)
void	arm10_tlb_flushID_SE	(u_int);
void	arm10_tlb_flushI_SE	(u_int);

void	arm10_context_switch	(u_int);

void	arm10_setup		(char *);
#endif

#if defined(CPU_ARM9E) || defined (CPU_ARM10) || defined(CPU_SHEEVA)
void	armv5_ec_setttb			(u_int, bool);

void	armv5_ec_icache_sync_all	(void);
void	armv5_ec_icache_sync_range	(vaddr_t, vsize_t);

void	armv5_ec_dcache_wbinv_all	(void);
void	armv5_ec_dcache_wbinv_range	(vaddr_t, vsize_t);
void	armv5_ec_dcache_inv_range	(vaddr_t, vsize_t);
void	armv5_ec_dcache_wb_range	(vaddr_t, vsize_t);

void	armv5_ec_idcache_wbinv_all	(void);
void	armv5_ec_idcache_wbinv_range	(vaddr_t, vsize_t);
#endif

#if defined (CPU_ARM10) || defined (CPU_ARM11MPCORE)
void	armv5_setttb		(u_int, bool);

void	armv5_icache_sync_all	(void);
void	armv5_icache_sync_range	(vaddr_t, vsize_t);

void	armv5_dcache_wbinv_all	(void);
void	armv5_dcache_wbinv_range (vaddr_t, vsize_t);
void	armv5_dcache_inv_range	(vaddr_t, vsize_t);
void	armv5_dcache_wb_range	(vaddr_t, vsize_t);

void	armv5_idcache_wbinv_all	(void);
void	armv5_idcache_wbinv_range (vaddr_t, vsize_t);

extern unsigned armv5_dcache_sets_max;
extern unsigned armv5_dcache_sets_inc;
extern unsigned armv5_dcache_index_max;
extern unsigned armv5_dcache_index_inc;
#endif

#if defined(CPU_ARM11MPCORE)
void	arm11mpcore_setup		(char *);
#endif

#if defined(CPU_ARM11) || defined(CPU_CORTEX)
void	arm11_setttb		(u_int, bool);

void	arm11_tlb_flushID_SE	(u_int);
void	arm11_tlb_flushI_SE	(u_int);

void	arm11_context_switch	(u_int);

void	arm11_cpu_sleep		(int);
void	arm11_setup		(char *string);
void	arm11_tlb_flushID	(void);
void	arm11_tlb_flushI	(void);
void	arm11_tlb_flushD	(void);
void	arm11_tlb_flushD_SE	(u_int va);

void	armv11_dcache_wbinv_all (void);
void	armv11_idcache_wbinv_all(void);

void	arm11_drain_writebuf	(void);
void	arm11_sleep		(int);

void	armv6_setttb		(u_int, bool);

void	armv6_icache_sync_all	(void);
void	armv6_icache_sync_range	(vaddr_t, vsize_t);

void	armv6_dcache_wbinv_all	(void);
void	armv6_dcache_wbinv_range (vaddr_t, vsize_t);
void	armv6_dcache_inv_range	(vaddr_t, vsize_t);
void	armv6_dcache_wb_range	(vaddr_t, vsize_t);

void	armv6_idcache_wbinv_all	(void);
void	armv6_idcache_wbinv_range (vaddr_t, vsize_t);
#endif

#if defined(CPU_CORTEX)
void	armv7_setttb(u_int, bool);

void	armv7_icache_sync_range(vaddr_t, vsize_t);
void	armv7_dcache_wb_range(vaddr_t, vsize_t);
void	armv7_dcache_wbinv_range(vaddr_t, vsize_t);
void	armv7_dcache_inv_range(vaddr_t, vsize_t);
void	armv7_idcache_wbinv_range(vaddr_t, vsize_t);

void 	armv7_dcache_wbinv_all (void);
void	armv7_idcache_wbinv_all(void);
void	armv7_icache_sync_all(void);
void	armv7_cpu_sleep(int);
void	armv7_context_switch(u_int);
void	armv7_tlb_flushID_SE(u_int);
void	armv7_setup		(char *string);
#endif


#if defined(CPU_ARM1136) || defined(CPU_ARM1176)
void	arm11x6_setttb			(u_int, bool);
void	arm11x6_idcache_wbinv_all	(void);
void	arm11x6_dcache_wbinv_all	(void);
void	arm11x6_icache_sync_all		(void);
void	arm11x6_flush_prefetchbuf	(void);
void	arm11x6_icache_sync_range	(vaddr_t, vsize_t);
void	arm11x6_idcache_wbinv_range	(vaddr_t, vsize_t);
void	arm11x6_setup			(char *string);
void	arm11x6_sleep			(int);	/* no ref. for errata */
#endif
#if defined(CPU_ARM1136)
void	arm1136_sleep_rev0		(int);	/* for errata 336501 */
#endif


#if defined(CPU_ARM9) || defined(CPU_ARM9E) || defined(CPU_ARM10) || \
    defined(CPU_SA110) || defined(CPU_SA1100) || defined(CPU_SA1110) || \
    defined(CPU_FA526) || \
    defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(__CPU_XSCALE_PXA2XX) || defined(CPU_XSCALE_IXP425) || \
    defined(CPU_CORTEX) || defined(CPU_SHEEVA)

void	armv4_tlb_flushID	(void);
void	armv4_tlb_flushI	(void);
void	armv4_tlb_flushD	(void);
void	armv4_tlb_flushD_SE	(u_int);

void	armv4_drain_writebuf	(void);
#endif

#if defined(CPU_IXP12X0)
void	ixp12x0_drain_readbuf	(void);
void	ixp12x0_context_switch	(u_int);
void	ixp12x0_setup		(char *);
#endif

#if defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(__CPU_XSCALE_PXA2XX) || defined(CPU_XSCALE_IXP425) || \
    defined(CPU_CORTEX)

void	xscale_cpwait		(void);
#define	cpu_cpwait()		cpufuncs.cf_cpwait()

void	xscale_cpu_sleep	(int);

u_int	xscale_control		(u_int, u_int);

void	xscale_setttb		(u_int, bool);

void	xscale_tlb_flushID_SE	(u_int);

void	xscale_cache_flushID	(void);
void	xscale_cache_flushI	(void);
void	xscale_cache_flushD	(void);
void	xscale_cache_flushD_SE	(u_int);

void	xscale_cache_cleanID	(void);
void	xscale_cache_cleanD	(void);
void	xscale_cache_cleanD_E	(u_int);

void	xscale_cache_clean_minidata (void);

void	xscale_cache_purgeID	(void);
void	xscale_cache_purgeID_E	(u_int);
void	xscale_cache_purgeD	(void);
void	xscale_cache_purgeD_E	(u_int);

void	xscale_cache_syncI	(void);
void	xscale_cache_cleanID_rng (vaddr_t, vsize_t);
void	xscale_cache_cleanD_rng	(vaddr_t, vsize_t);
void	xscale_cache_purgeID_rng (vaddr_t, vsize_t);
void	xscale_cache_purgeD_rng	(vaddr_t, vsize_t);
void	xscale_cache_syncI_rng	(vaddr_t, vsize_t);
void	xscale_cache_flushD_rng	(vaddr_t, vsize_t);

void	xscale_context_switch	(u_int);

void	xscale_setup		(char *);
#endif	/* CPU_XSCALE_80200 || CPU_XSCALE_80321 || __CPU_XSCALE_PXA2XX || CPU_XSCALE_IXP425 || CPU_CORTEX */

#if defined(CPU_SHEEVA)
void	sheeva_dcache_wbinv_range (vaddr_t, vsize_t);
void	sheeva_dcache_inv_range	(vaddr_t, vsize_t);
void	sheeva_dcache_wb_range	(vaddr_t, vsize_t);
void	sheeva_idcache_wbinv_range (vaddr_t, vsize_t);
void	sheeva_setup(char *);
void	sheeva_cpu_sleep(int);
#endif

#define tlb_flush	cpu_tlb_flushID
#define setttb		cpu_setttb
#define drain_writebuf	cpu_drain_writebuf

#ifndef cpu_cpwait
#define	cpu_cpwait()
#endif

/*
 * Macros for manipulating CPU interrupts
 */
#ifdef __PROG32
static __inline u_int32_t __set_cpsr_c(uint32_t bic, uint32_t eor) __attribute__((__unused__));
static __inline u_int32_t disable_interrupts(uint32_t mask) __attribute__((__unused__));
static __inline u_int32_t enable_interrupts(uint32_t mask) __attribute__((__unused__));

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
	uint32_t	ret, tmp;
	mask &= (I32_bit | F32_bit);

	__asm volatile(
		"mrs     %0, cpsr\n"	/* Get the CPSR */
		"bic	 %1, %0, %2\n"	/* Clear bits */
		"msr     cpsr_c, %1\n"	/* Set the control field of CPSR */
	: "=&r" (ret), "=&r" (tmp)
	: "r" (mask)
	: "memory");

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

/* PRIMARY CACHE VARIABLES */
struct arm_cache_info {
	u_int icache_size;
	u_int icache_line_size;
	u_int icache_ways;
	u_int icache_sets;

	u_int dcache_size;
	u_int dcache_line_size;
	u_int dcache_ways;
	u_int dcache_sets;

	u_int cache_type;
	bool cache_unified;
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

#endif	/* _ARM32_CPUFUNC_H_ */

/* End of cpufunc.h */
