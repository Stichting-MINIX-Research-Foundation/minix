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

#ifndef _ARM_CPUFUNC_PROTO_H_
#define _ARM_CPUFUNC_PROTO_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <arm/armreg.h>
#include <arm/cpuconf.h>

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
void	arm67_tlb_purge		(vaddr_t);
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
void	arm7tdmi_tlb_flushID_SE	(vaddr_t);
void	arm7tdmi_cache_flushID	(void);
void	arm7tdmi_context_switch	(u_int);
#endif /* CPU_ARM7TDMI */

#ifdef CPU_ARM8
void	arm8_setttb		(u_int, bool);
void	arm8_tlb_flushID	(void);
void	arm8_tlb_flushID_SE	(vaddr_t);
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
void	fa526_tlb_flushI_SE	(vaddr_t);
void	fa526_tlb_flushID_SE	(vaddr_t);
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

void	sa1_tlb_flushID_SE	(vaddr_t);

void	sa1_cache_flushID	(void);
void	sa1_cache_flushI	(void);
void	sa1_cache_flushD	(void);
void	sa1_cache_flushD_SE	(vaddr_t);

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

void	arm9_tlb_flushID_SE	(vaddr_t);

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
void	arm10_tlb_flushID_SE	(vaddr_t);
void	arm10_tlb_flushI_SE	(vaddr_t);

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

#if defined(CPU_ARM11)
#if defined(ARM_MMU_EXTENDED)
void	arm11_setttb		(u_int, tlb_asid_t);
void	arm11_context_switch	(u_int, tlb_asid_t);
#else
void	arm11_setttb		(u_int, bool);
void	arm11_context_switch	(u_int);
#endif

void	arm11_cpu_sleep		(int);
void	arm11_setup		(char *string);
void	arm11_tlb_flushID	(void);
void	arm11_tlb_flushI	(void);
void	arm11_tlb_flushD	(void);
void	arm11_tlb_flushID_SE	(vaddr_t);
void	arm11_tlb_flushI_SE	(vaddr_t);
void	arm11_tlb_flushD_SE	(vaddr_t);

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

#if defined(CPU_ARMV7)
#if defined(ARM_MMU_EXTENDED)
void	armv7_setttb(u_int, tlb_asid_t);
void	armv7_context_switch(u_int, tlb_asid_t);
#else
void	armv7_setttb(u_int, bool);
void	armv7_context_switch(u_int);
#endif

void	armv7_icache_sync_range(vaddr_t, vsize_t);
void	armv7_icache_sync_all(void);

void	armv7_dcache_inv_range(vaddr_t, vsize_t);
void	armv7_dcache_wb_range(vaddr_t, vsize_t);
void	armv7_dcache_wbinv_range(vaddr_t, vsize_t);
void 	armv7_dcache_wbinv_all(void);

void	armv7_idcache_wbinv_range(vaddr_t, vsize_t);
void	armv7_idcache_wbinv_all(void);

void	armv7_tlb_flushID(void);
void	armv7_tlb_flushI(void);
void	armv7_tlb_flushD(void);

void	armv7_tlb_flushID_SE(vaddr_t);
void	armv7_tlb_flushI_SE(vaddr_t);
void	armv7_tlb_flushD_SE(vaddr_t);

void	armv7_cpu_sleep(int);
void	armv7_drain_writebuf(void);
void	armv7_setup(char *string);
#endif /* CPU_ARMV7 */

#if defined(CPU_PJ4B)
void	pj4b_cpu_sleep(int);
void	pj4bv7_setup(char *string);
void	pj4b_config(void);
void	pj4b_io_coherency_barrier(vaddr_t, paddr_t, vsize_t);
void	pj4b_dcache_cfu_inv_range(vaddr_t, vsize_t);
void	pj4b_dcache_cfu_wb_range(vaddr_t, vsize_t);
void	pj4b_dcache_cfu_wbinv_range(vaddr_t, vsize_t);
#endif /* CPU_PJ4B */

#if defined(CPU_ARM1136) || defined(CPU_ARM1176)
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
    defined(CPU_FA526) || defined(CPU_XSCALE) || defined(CPU_SHEEVA)

void	armv4_tlb_flushID	(void);
void	armv4_tlb_flushI	(void);
void	armv4_tlb_flushD	(void);
void	armv4_tlb_flushD_SE	(vaddr_t);

void	armv4_drain_writebuf	(void);
#endif

#if defined(CPU_IXP12X0)
void	ixp12x0_drain_readbuf	(void);
void	ixp12x0_context_switch	(u_int);
void	ixp12x0_setup		(char *);
#endif

#if defined(CPU_XSCALE)
void	xscale_cpwait		(void);

void	xscale_cpu_sleep	(int);

u_int	xscale_control		(u_int, u_int);

void	xscale_setttb		(u_int, bool);

void	xscale_tlb_flushID_SE	(vaddr_t);

void	xscale_cache_flushID	(void);
void	xscale_cache_flushI	(void);
void	xscale_cache_flushD	(void);
void	xscale_cache_flushD_SE	(vaddr_t);

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
#endif	/* CPU_XSCALE */

#if defined(CPU_SHEEVA)
void	sheeva_dcache_wbinv_range (vaddr_t, vsize_t);
void	sheeva_dcache_inv_range	(vaddr_t, vsize_t);
void	sheeva_dcache_wb_range	(vaddr_t, vsize_t);
void	sheeva_idcache_wbinv_range (vaddr_t, vsize_t);
void	sheeva_setup(char *);
void	sheeva_cpu_sleep(int);

void	sheeva_sdcache_inv_range(vaddr_t, paddr_t, vsize_t);
void	sheeva_sdcache_wb_range(vaddr_t, paddr_t, vsize_t);
void	sheeva_sdcache_wbinv_range(vaddr_t, paddr_t, vsize_t);
void	sheeva_sdcache_wbinv_all(void);
#endif

#endif /* _KERNEL */

#endif	/* _ARM_CPUFUNC_PROTO_H_ */
