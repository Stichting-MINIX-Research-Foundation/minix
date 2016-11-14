/*	$NetBSD: cpufunc.c,v 1.156 2015/07/02 08:33:31 skrll Exp $	*/

/*
 * arm7tdmi support code Copyright (c) 2001 John Fremlin
 * arm8 support code Copyright (c) 1997 ARM Limited
 * arm8 support code Copyright (c) 1997 Causality Limited
 * arm9 support code Copyright (C) 2001 ARM Ltd
 * arm11 support code Copyright (c) 2007 Microsoft
 * cortexa8 support code Copyright (c) 2008 3am Software Foundry
 * cortexa8 improvements Copyright (c) Goeran Weinholt
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
 * cpufuncs.c
 *
 * C functions for supporting CPU / MMU / TLB specific operations.
 *
 * Created	: 30/01/97
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cpufunc.c,v 1.156 2015/07/02 08:33:31 skrll Exp $");

#include "opt_compat_netbsd.h"
#include "opt_cpuoptions.h"
#include "opt_perfctrs.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/systm.h>
#include <machine/cpu.h>
#include <machine/bootconfig.h>
#include <arch/arm/arm/disassem.h>

#include <uvm/uvm.h>

#include <arm/cpufunc_proto.h>
#include <arm/cpuconf.h>
#include <arm/locore.h>

#ifdef CPU_XSCALE_80200
#include <arm/xscale/i80200reg.h>
#include <arm/xscale/i80200var.h>
#endif

#ifdef CPU_XSCALE_80321
#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>
#endif

#ifdef CPU_XSCALE_IXP425
#include <arm/xscale/ixp425reg.h>
#include <arm/xscale/ixp425var.h>
#endif

#if defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321)
#include <arm/xscale/xscalereg.h>
#endif

#if defined(CPU_PJ4B)
#include "opt_cputypes.h"
#include "opt_mvsoc.h"
#include <machine/bus_defs.h>
#if defined(ARMADAXP)
#include <arm/marvell/armadaxpreg.h>
#include <arm/marvell/armadaxpvar.h>
#endif
#endif

#if defined(PERFCTRS)
struct arm_pmc_funcs *arm_pmc;
#endif

#if defined(CPU_ARMV7) && (defined(CPU_ARMV6) || defined(CPU_PRE_ARMV6))
bool cpu_armv7_p;
#endif

#if defined(CPU_ARMV6) && (defined(CPU_ARMV7) || defined(CPU_PRE_ARMV6))
bool cpu_armv6_p;
#endif


/* PRIMARY CACHE VARIABLES */
#if (ARM_MMU_V6 + ARM_MMU_V7) != 0
u_int	arm_cache_prefer_mask;
#endif
struct	arm_cache_info arm_pcache;
struct	arm_cache_info arm_scache;

u_int	arm_dcache_align;
u_int	arm_dcache_align_mask;

/* 1 == use cpu_sleep(), 0 == don't */
int cpu_do_powersave;

#ifdef CPU_ARM2
struct cpu_functions arm2_cpufuncs = {
	/* CPU functions */

	.cf_id			= arm2_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= (void *)cpufunc_nullop,

	/* TLB functions */

	.cf_tlb_flushID		= cpufunc_nullop,
	.cf_tlb_flushID_SE	= (void *)cpufunc_nullop,
	.cf_tlb_flushI		= cpufunc_nullop,
	.cf_tlb_flushI_SE	= (void *)cpufunc_nullop,
	.cf_tlb_flushD		= cpufunc_nullop,
	.cf_tlb_flushD_SE	= (void *)cpufunc_nullop,

	/* Cache operations */

	.cf_icache_sync_all	= cpufunc_nullop,
	.cf_icache_sync_range	= (void *) cpufunc_nullop,

	.cf_dcache_wbinv_all	= arm3_cache_flush,
	.cf_dcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_dcache_inv_range	= (void *)cpufunc_nullop,
	.cf_dcache_wb_range	= (void *)cpufunc_nullop,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= cpufunc_nullop,
	.cf_idcache_wbinv_range	= (void *)cpufunc_nullop,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= cpufunc_nullop,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= early_abort_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_setup		= (void *)cpufunc_nullop

};
#endif	/* CPU_ARM2 */

#ifdef CPU_ARM250
struct cpu_functions arm250_cpufuncs = {
	/* CPU functions */

	.cf_id			= arm250_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= (void *)cpufunc_nullop,

	/* TLB functions */

	.cf_tlb_flushID		= cpufunc_nullop,
	.cf_tlb_flushID_SE	= (void *)cpufunc_nullop,
	.cf_tlb_flushI		= cpufunc_nullop,
	.cf_tlb_flushI_SE	= (void *)cpufunc_nullop,
	.cf_tlb_flushD		= cpufunc_nullop,
	.cf_tlb_flushD_SE	= (void *)cpufunc_nullop,

	/* Cache operations */

	.cf_icache_sync_all	= cpufunc_nullop,
	.cf_icache_sync_range	= (void *) cpufunc_nullop,

	.cf_dcache_wbinv_all	= arm3_cache_flush,
	.cf_dcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_dcache_inv_range	= (void *)cpufunc_nullop,
	.cf_dcache_wb_range	= (void *)cpufunc_nullop,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= cpufunc_nullop,
	.cf_idcache_wbinv_range	= (void *)cpufunc_nullop,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= cpufunc_nullop,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= early_abort_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_setup		= (void *)cpufunc_nullop

};
#endif	/* CPU_ARM250 */

#ifdef CPU_ARM3
struct cpu_functions arm3_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= arm3_control,

	/* TLB functions */

	.cf_tlb_flushID		= cpufunc_nullop,
	.cf_tlb_flushID_SE	= (void *)cpufunc_nullop,
	.cf_tlb_flushI		= cpufunc_nullop,
	.cf_tlb_flushI_SE	= (void *)cpufunc_nullop,
	.cf_tlb_flushD		= cpufunc_nullop,
	.cf_tlb_flushD_SE	= (void *)cpufunc_nullop,

	/* Cache operations */

	.cf_icache_sync_all	= cpufunc_nullop,
	.cf_icache_sync_range	= (void *) cpufunc_nullop,

	.cf_dcache_wbinv_all	= arm3_cache_flush,
	.cf_dcache_wbinv_range	= (void *)arm3_cache_flush,
	.cf_dcache_inv_range	= (void *)arm3_cache_flush,
	.cf_dcache_wb_range	= (void *)cpufunc_nullop,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm3_cache_flush,
	.cf_idcache_wbinv_range	= (void *)arm3_cache_flush,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= cpufunc_nullop,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= early_abort_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_setup		= (void *)cpufunc_nullop

};
#endif	/* CPU_ARM3 */

#ifdef CPU_ARM6
struct cpu_functions arm6_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm67_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm67_tlb_flush,
	.cf_tlb_flushID_SE	= arm67_tlb_purge,
	.cf_tlb_flushI		= arm67_tlb_flush,
	.cf_tlb_flushI_SE	= arm67_tlb_purge,
	.cf_tlb_flushD		= arm67_tlb_flush,
	.cf_tlb_flushD_SE	= arm67_tlb_purge,

	/* Cache operations */

	.cf_icache_sync_all	= cpufunc_nullop,
	.cf_icache_sync_range	= (void *) cpufunc_nullop,

	.cf_dcache_wbinv_all	= arm67_cache_flush,
	.cf_dcache_wbinv_range	= (void *)arm67_cache_flush,
	.cf_dcache_inv_range	= (void *)arm67_cache_flush,
	.cf_dcache_wb_range	= (void *)cpufunc_nullop,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm67_cache_flush,
	.cf_idcache_wbinv_range	= (void *)arm67_cache_flush,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= cpufunc_nullop,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

#ifdef ARM6_LATE_ABORT
	.cf_dataabt_fixup	= late_abort_fixup,
#else
	.cf_dataabt_fixup	= early_abort_fixup,
#endif
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm67_context_switch,

	.cf_setup		= arm6_setup

};
#endif	/* CPU_ARM6 */

#ifdef CPU_ARM7
struct cpu_functions arm7_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm67_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm67_tlb_flush,
	.cf_tlb_flushID_SE	= arm67_tlb_purge,
	.cf_tlb_flushI		= arm67_tlb_flush,
	.cf_tlb_flushI_SE	= arm67_tlb_purge,
	.cf_tlb_flushD		= arm67_tlb_flush,
	.cf_tlb_flushD_SE	= arm67_tlb_purge,

	/* Cache operations */

	.cf_icache_sync_all	= cpufunc_nullop,
	.cf_icache_sync_range	= (void *)cpufunc_nullop,

	.cf_dcache_wbinv_all	= arm67_cache_flush,
	.cf_dcache_wbinv_range	= (void *)arm67_cache_flush,
	.cf_dcache_inv_range	= (void *)arm67_cache_flush,
	.cf_dcache_wb_range	= (void *)cpufunc_nullop,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm67_cache_flush,
	.cf_idcache_wbinv_range	= (void *)arm67_cache_flush,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= cpufunc_nullop,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= late_abort_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm67_context_switch,

	.cf_setup		= arm7_setup

};
#endif	/* CPU_ARM7 */

#ifdef CPU_ARM7TDMI
struct cpu_functions arm7tdmi_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm7tdmi_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm7tdmi_tlb_flushID,
	.cf_tlb_flushID_SE	= arm7tdmi_tlb_flushID_SE,
	.cf_tlb_flushI		= arm7tdmi_tlb_flushID,
	.cf_tlb_flushI_SE	= arm7tdmi_tlb_flushID_SE,
	.cf_tlb_flushD		= arm7tdmi_tlb_flushID,
	.cf_tlb_flushD_SE	= arm7tdmi_tlb_flushID_SE,

	/* Cache operations */

	.cf_icache_sync_all	= cpufunc_nullop,
	.cf_icache_sync_range	= (void *)cpufunc_nullop,

	.cf_dcache_wbinv_all	= arm7tdmi_cache_flushID,
	.cf_dcache_wbinv_range	= (void *)arm7tdmi_cache_flushID,
	.cf_dcache_inv_range	= (void *)arm7tdmi_cache_flushID,
	.cf_dcache_wb_range	= (void *)cpufunc_nullop,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm7tdmi_cache_flushID,
	.cf_idcache_wbinv_range	= (void *)arm7tdmi_cache_flushID,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= cpufunc_nullop,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= late_abort_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm7tdmi_context_switch,

	.cf_setup		= arm7tdmi_setup

};
#endif	/* CPU_ARM7TDMI */

#ifdef CPU_ARM8
struct cpu_functions arm8_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm8_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm8_tlb_flushID,
	.cf_tlb_flushID_SE	= arm8_tlb_flushID_SE,
	.cf_tlb_flushI		= arm8_tlb_flushID,
	.cf_tlb_flushI_SE	= arm8_tlb_flushID_SE,
	.cf_tlb_flushD		= arm8_tlb_flushID,
	.cf_tlb_flushD_SE	= arm8_tlb_flushID_SE,

	/* Cache operations */

	.cf_icache_sync_all	= cpufunc_nullop,
	.cf_icache_sync_range	= (void *)cpufunc_nullop,

	.cf_dcache_wbinv_all	= arm8_cache_purgeID,
	.cf_dcache_wbinv_range	= (void *)arm8_cache_purgeID,
/*XXX*/	.cf_dcache_inv_range	= (void *)arm8_cache_purgeID,
	.cf_dcache_wb_range	= (void *)arm8_cache_cleanID,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm8_cache_purgeID,
	.cf_idcache_wbinv_range = (void *)arm8_cache_purgeID,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= cpufunc_nullop,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm8_context_switch,

	.cf_setup		= arm8_setup
};
#endif	/* CPU_ARM8 */

#ifdef CPU_ARM9
struct cpu_functions arm9_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm9_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= arm9_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= (void *)armv4_tlb_flushI,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= arm9_icache_sync_all,
	.cf_icache_sync_range	= arm9_icache_sync_range,

	.cf_dcache_wbinv_all	= arm9_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= arm9_dcache_wbinv_range,
/*XXX*/	.cf_dcache_inv_range	= arm9_dcache_wbinv_range,
	.cf_dcache_wb_range	= arm9_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm9_idcache_wbinv_all,
	.cf_idcache_wbinv_range = arm9_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm9_context_switch,

	.cf_setup		= arm9_setup

};
#endif /* CPU_ARM9 */

#if defined(CPU_ARM9E) || defined(CPU_ARM10)
struct cpu_functions armv5_ec_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= armv5_ec_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= arm10_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= arm10_tlb_flushI_SE,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= armv5_ec_icache_sync_all,
	.cf_icache_sync_range	= armv5_ec_icache_sync_range,

	.cf_dcache_wbinv_all	= armv5_ec_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= armv5_ec_dcache_wbinv_range,
/*XXX*/	.cf_dcache_inv_range	= armv5_ec_dcache_wbinv_range,
	.cf_dcache_wb_range	= armv5_ec_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= armv5_ec_idcache_wbinv_all,
	.cf_idcache_wbinv_range = armv5_ec_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm10_context_switch,

	.cf_setup		= arm10_setup

};
#endif /* CPU_ARM9E || CPU_ARM10 */

#ifdef CPU_ARM10
struct cpu_functions arm10_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= armv5_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= arm10_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= arm10_tlb_flushI_SE,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= armv5_icache_sync_all,
	.cf_icache_sync_range	= armv5_icache_sync_range,

	.cf_dcache_wbinv_all	= armv5_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= armv5_dcache_wbinv_range,
/*XXX*/	.cf_dcache_inv_range	= armv5_dcache_wbinv_range,
	.cf_dcache_wb_range	= armv5_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= armv5_idcache_wbinv_all,
	.cf_idcache_wbinv_range = armv5_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm10_context_switch,

	.cf_setup		= arm10_setup

};
#endif /* CPU_ARM10 */

#ifdef CPU_ARM11
struct cpu_functions arm11_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm11_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm11_tlb_flushID,
	.cf_tlb_flushID_SE	= arm11_tlb_flushID_SE,
	.cf_tlb_flushI		= arm11_tlb_flushI,
	.cf_tlb_flushI_SE	= arm11_tlb_flushI_SE,
	.cf_tlb_flushD		= arm11_tlb_flushD,
	.cf_tlb_flushD_SE	= arm11_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= armv6_icache_sync_all,
	.cf_icache_sync_range	= armv6_icache_sync_range,

	.cf_dcache_wbinv_all	= armv6_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= armv6_dcache_wbinv_range,
	.cf_dcache_inv_range	= armv6_dcache_inv_range,
	.cf_dcache_wb_range	= armv6_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= armv6_idcache_wbinv_all,
	.cf_idcache_wbinv_range = armv6_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= arm11_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= arm11_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm11_context_switch,

	.cf_setup		= arm11_setup

};
#endif /* CPU_ARM11 */

#ifdef CPU_ARM1136
struct cpu_functions arm1136_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm11_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm11_tlb_flushID,
	.cf_tlb_flushID_SE	= arm11_tlb_flushID_SE,
	.cf_tlb_flushI		= arm11_tlb_flushI,
	.cf_tlb_flushI_SE	= arm11_tlb_flushI_SE,
	.cf_tlb_flushD		= arm11_tlb_flushD,
	.cf_tlb_flushD_SE	= arm11_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= arm11x6_icache_sync_all,	/* 411920 */
	.cf_icache_sync_range	= arm11x6_icache_sync_range,	/* 371025 */

	.cf_dcache_wbinv_all	= arm11x6_dcache_wbinv_all,	/* 411920 */
	.cf_dcache_wbinv_range	= armv6_dcache_wbinv_range,
	.cf_dcache_inv_range	= armv6_dcache_inv_range,
	.cf_dcache_wb_range	= armv6_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm11x6_idcache_wbinv_all,	/* 411920 */
	.cf_idcache_wbinv_range = arm11x6_idcache_wbinv_range,	/* 371025 */

	/* Other functions */

	.cf_flush_prefetchbuf	= arm11x6_flush_prefetchbuf,
	.cf_drain_writebuf	= arm11_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= arm11_sleep,	/* arm1136_sleep_rev0 */

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm11_context_switch,

	.cf_setup		= arm11x6_setup

};
#endif /* CPU_ARM1136 */

#ifdef CPU_ARM1176
struct cpu_functions arm1176_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm11_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm11_tlb_flushID,
	.cf_tlb_flushID_SE	= arm11_tlb_flushID_SE,
	.cf_tlb_flushI		= arm11_tlb_flushI,
	.cf_tlb_flushI_SE	= arm11_tlb_flushI_SE,
	.cf_tlb_flushD		= arm11_tlb_flushD,
	.cf_tlb_flushD_SE	= arm11_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= arm11x6_icache_sync_all,	/* 415045 */
	.cf_icache_sync_range	= arm11x6_icache_sync_range,	/* 371367 */

	.cf_dcache_wbinv_all	= arm11x6_dcache_wbinv_all,	/* 415045 */
	.cf_dcache_wbinv_range	= armv6_dcache_wbinv_range,
	.cf_dcache_inv_range	= armv6_dcache_inv_range,
	.cf_dcache_wb_range	= armv6_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= arm11x6_idcache_wbinv_all,	/* 415045 */
	.cf_idcache_wbinv_range = arm11x6_idcache_wbinv_range,	/* 371367 */

	/* Other functions */

	.cf_flush_prefetchbuf	= arm11x6_flush_prefetchbuf,
	.cf_drain_writebuf	= arm11_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= arm11x6_sleep,		/* no ref. */

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm11_context_switch,

	.cf_setup		= arm11x6_setup

};
#endif /* CPU_ARM1176 */


#ifdef CPU_ARM11MPCORE
struct cpu_functions arm11mpcore_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= arm11_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= arm11_tlb_flushID,
	.cf_tlb_flushID_SE	= arm11_tlb_flushID_SE,
	.cf_tlb_flushI		= arm11_tlb_flushI,
	.cf_tlb_flushI_SE	= arm11_tlb_flushI_SE,
	.cf_tlb_flushD		= arm11_tlb_flushD,
	.cf_tlb_flushD_SE	= arm11_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= armv6_icache_sync_all,
	.cf_icache_sync_range	= armv5_icache_sync_range,

	.cf_dcache_wbinv_all	= armv6_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= armv5_dcache_wbinv_range,
	.cf_dcache_inv_range	= armv5_dcache_inv_range,
	.cf_dcache_wb_range	= armv5_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= armv6_idcache_wbinv_all,
	.cf_idcache_wbinv_range = armv5_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= arm11_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= arm11_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm11_context_switch,

	.cf_setup		= arm11mpcore_setup

};
#endif /* CPU_ARM11MPCORE */

#ifdef CPU_SA110
struct cpu_functions sa110_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= sa1_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= sa1_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= (void *)armv4_tlb_flushI,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= sa1_cache_syncI,
	.cf_icache_sync_range	= sa1_cache_syncI_rng,

	.cf_dcache_wbinv_all	= sa1_cache_purgeD,
	.cf_dcache_wbinv_range	= sa1_cache_purgeD_rng,
/*XXX*/	.cf_dcache_inv_range	= sa1_cache_purgeD_rng,
	.cf_dcache_wb_range	= sa1_cache_cleanD_rng,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= sa1_cache_purgeID,
	.cf_idcache_wbinv_range	= sa1_cache_purgeID_rng,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= sa110_context_switch,

	.cf_setup		= sa110_setup
};
#endif	/* CPU_SA110 */

#if defined(CPU_SA1100) || defined(CPU_SA1110)
struct cpu_functions sa11x0_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= sa1_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= sa1_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= (void *)armv4_tlb_flushI,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= sa1_cache_syncI,
	.cf_icache_sync_range	= sa1_cache_syncI_rng,

	.cf_dcache_wbinv_all	= sa1_cache_purgeD,
	.cf_dcache_wbinv_range	= sa1_cache_purgeD_rng,
/*XXX*/	.cf_dcache_inv_range	= sa1_cache_purgeD_rng,
	.cf_dcache_wb_range	= sa1_cache_cleanD_rng,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= sa1_cache_purgeID,
	.cf_idcache_wbinv_range	= sa1_cache_purgeID_rng,

	/* Other functions */

	.cf_flush_prefetchbuf	= sa11x0_drain_readbuf,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= sa11x0_cpu_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= sa11x0_context_switch,

	.cf_setup		= sa11x0_setup
};
#endif	/* CPU_SA1100 || CPU_SA1110 */

#if defined(CPU_FA526)
struct cpu_functions fa526_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= fa526_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= fa526_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= fa526_tlb_flushI_SE,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= fa526_icache_sync_all,
	.cf_icache_sync_range	= fa526_icache_sync_range,

	.cf_dcache_wbinv_all	= fa526_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= fa526_dcache_wbinv_range,
	.cf_dcache_inv_range	= fa526_dcache_inv_range,
	.cf_dcache_wb_range	= fa526_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= fa526_idcache_wbinv_all,
	.cf_idcache_wbinv_range	= fa526_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= fa526_flush_prefetchbuf,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= fa526_flush_brnchtgt_E,

	.cf_sleep		= fa526_cpu_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= fa526_context_switch,

	.cf_setup		= fa526_setup
};
#endif	/* CPU_FA526 */

#ifdef CPU_IXP12X0
struct cpu_functions ixp12x0_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= sa1_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= sa1_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= (void *)armv4_tlb_flushI,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= sa1_cache_syncI,
	.cf_icache_sync_range	= sa1_cache_syncI_rng,

	.cf_dcache_wbinv_all	= sa1_cache_purgeD,
	.cf_dcache_wbinv_range	= sa1_cache_purgeD_rng,
/*XXX*/	.cf_dcache_inv_range	= sa1_cache_purgeD_rng,
	.cf_dcache_wb_range	= sa1_cache_cleanD_rng,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= sa1_cache_purgeID,
	.cf_idcache_wbinv_range	= sa1_cache_purgeID_rng,

	/* Other functions */

	.cf_flush_prefetchbuf	= ixp12x0_drain_readbuf,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)cpufunc_nullop,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= ixp12x0_context_switch,

	.cf_setup		= ixp12x0_setup
};
#endif	/* CPU_IXP12X0 */

#if defined(CPU_XSCALE)
struct cpu_functions xscale_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= xscale_cpwait,

	/* MMU functions */

	.cf_control		= xscale_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= xscale_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= xscale_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= (void *)armv4_tlb_flushI,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= xscale_cache_syncI,
	.cf_icache_sync_range	= xscale_cache_syncI_rng,

	.cf_dcache_wbinv_all	= xscale_cache_purgeD,
	.cf_dcache_wbinv_range	= xscale_cache_purgeD_rng,
	.cf_dcache_inv_range	= xscale_cache_flushD_rng,
	.cf_dcache_wb_range	= xscale_cache_cleanD_rng,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= xscale_cache_purgeID,
	.cf_idcache_wbinv_range = xscale_cache_purgeID_rng,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= xscale_cpu_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= xscale_context_switch,

	.cf_setup		= xscale_setup
};
#endif /* CPU_XSCALE */

#if defined(CPU_ARMV7)
struct cpu_functions armv7_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= armv7_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv7_tlb_flushID,
	.cf_tlb_flushID_SE	= armv7_tlb_flushID_SE,
	.cf_tlb_flushI		= armv7_tlb_flushI,
	.cf_tlb_flushI_SE	= armv7_tlb_flushI_SE,
	.cf_tlb_flushD		= armv7_tlb_flushD,
	.cf_tlb_flushD_SE	= armv7_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= armv7_icache_sync_all,
	.cf_dcache_wbinv_all	= armv7_dcache_wbinv_all,

	.cf_dcache_inv_range	= armv7_dcache_inv_range,
	.cf_dcache_wb_range	= armv7_dcache_wb_range,
	.cf_dcache_wbinv_range	= armv7_dcache_wbinv_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_icache_sync_range	= armv7_icache_sync_range,
	.cf_idcache_wbinv_range = armv7_idcache_wbinv_range,


	.cf_idcache_wbinv_all	= armv7_idcache_wbinv_all,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv7_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= armv7_cpu_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= armv7_context_switch,

	.cf_setup		= armv7_setup

};
#endif /* CPU_ARMV7 */

#ifdef CPU_PJ4B
struct cpu_functions pj4bv7_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= armv7_drain_writebuf,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= armv7_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv7_tlb_flushID,
	.cf_tlb_flushID_SE	= armv7_tlb_flushID_SE,
	.cf_tlb_flushI		= armv7_tlb_flushID,
	.cf_tlb_flushI_SE	= armv7_tlb_flushID_SE,
	.cf_tlb_flushD		= armv7_tlb_flushID,
	.cf_tlb_flushD_SE	= armv7_tlb_flushID_SE,

	/* Cache operations (see also pj4bv7_setup) */
	.cf_icache_sync_all	= armv7_idcache_wbinv_all,
	.cf_icache_sync_range	= armv7_icache_sync_range,

	.cf_dcache_wbinv_all	= armv7_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= armv7_dcache_wbinv_range,
	.cf_dcache_inv_range	= armv7_dcache_inv_range,
	.cf_dcache_wb_range	= armv7_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= armv7_idcache_wbinv_all,
	.cf_idcache_wbinv_range	= armv7_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv7_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= pj4b_cpu_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= armv7_context_switch,

	.cf_setup		= pj4bv7_setup
};
#endif /* CPU_PJ4B */

#ifdef CPU_SHEEVA
struct cpu_functions sheeva_cpufuncs = {
	/* CPU functions */

	.cf_id			= cpufunc_id,
	.cf_cpwait		= cpufunc_nullop,

	/* MMU functions */

	.cf_control		= cpufunc_control,
	.cf_domains		= cpufunc_domains,
	.cf_setttb		= armv5_ec_setttb,
	.cf_faultstatus		= cpufunc_faultstatus,
	.cf_faultaddress	= cpufunc_faultaddress,

	/* TLB functions */

	.cf_tlb_flushID		= armv4_tlb_flushID,
	.cf_tlb_flushID_SE	= arm10_tlb_flushID_SE,
	.cf_tlb_flushI		= armv4_tlb_flushI,
	.cf_tlb_flushI_SE	= arm10_tlb_flushI_SE,
	.cf_tlb_flushD		= armv4_tlb_flushD,
	.cf_tlb_flushD_SE	= armv4_tlb_flushD_SE,

	/* Cache operations */

	.cf_icache_sync_all	= armv5_ec_icache_sync_all,
	.cf_icache_sync_range	= armv5_ec_icache_sync_range,

	.cf_dcache_wbinv_all	= armv5_ec_dcache_wbinv_all,
	.cf_dcache_wbinv_range	= sheeva_dcache_wbinv_range,
	.cf_dcache_inv_range	= sheeva_dcache_inv_range,
	.cf_dcache_wb_range	= sheeva_dcache_wb_range,

	.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_inv_range	= (void *)cpufunc_nullop,
	.cf_sdcache_wb_range	= (void *)cpufunc_nullop,

	.cf_idcache_wbinv_all	= armv5_ec_idcache_wbinv_all,
	.cf_idcache_wbinv_range = sheeva_idcache_wbinv_range,

	/* Other functions */

	.cf_flush_prefetchbuf	= cpufunc_nullop,
	.cf_drain_writebuf	= armv4_drain_writebuf,
	.cf_flush_brnchtgt_C	= cpufunc_nullop,
	.cf_flush_brnchtgt_E	= (void *)cpufunc_nullop,

	.cf_sleep		= (void *)sheeva_cpu_sleep,

	/* Soft functions */

	.cf_dataabt_fixup	= cpufunc_null_fixup,
	.cf_prefetchabt_fixup	= cpufunc_null_fixup,

	.cf_context_switch	= arm10_context_switch,

	.cf_setup		= sheeva_setup
};
#endif /* CPU_SHEEVA */


/*
 * Global constants also used by locore.s
 */

struct cpu_functions cpufuncs;
u_int cputype;

#if defined(CPU_ARM7TDMI) || defined(CPU_ARM8) || defined(CPU_ARM9) || \
    defined(CPU_ARM9E) || defined(CPU_ARM10) || defined(CPU_FA526) || \
    defined(CPU_SHEEVA) || \
    defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(__CPU_XSCALE_PXA2XX) || defined(CPU_XSCALE_IXP425) || \
    defined(CPU_ARMV6) || defined(CPU_ARMV7)
static void get_cachetype_cp15(void);

/* Additional cache information local to this file.  Log2 of some of the
   above numbers.  */
static int	arm_dcache_log2_nsets;
static int	arm_dcache_log2_assoc;
static int	arm_dcache_log2_linesize;

#if (ARM_MMU_V6 + ARM_MMU_V7) > 0
static inline u_int
get_cachesize_cp15(int cssr)
{
#if defined(CPU_ARMV7)
	__asm volatile(".arch\tarmv7a");

	armreg_csselr_write(cssr);
	arm_isb();			 /* sync to the new cssr */

#else
	__asm volatile("mcr p15, 1, %0, c0, c0, 2" :: "r" (cssr) : "memory");
#endif
	return armreg_ccsidr_read();
}
#endif

#if (ARM_MMU_V6 + ARM_MMU_V7) > 0
static void
get_cacheinfo_clidr(struct arm_cache_info *info, u_int level, u_int clidr)
{
	u_int csid;

	if (clidr & 6) {
		csid = get_cachesize_cp15(level << 1); /* select dcache values */
		info->dcache_sets = CPU_CSID_NUMSETS(csid) + 1;
		info->dcache_ways = CPU_CSID_ASSOC(csid) + 1;
		info->dcache_line_size = 1U << (CPU_CSID_LEN(csid) + 4);
		info->dcache_way_size =
		    info->dcache_line_size * info->dcache_sets;
		info->dcache_size = info->dcache_way_size * info->dcache_ways;

		if (level == 0) {
			arm_dcache_log2_assoc = CPU_CSID_ASSOC(csid) + 1;
			arm_dcache_log2_linesize = CPU_CSID_LEN(csid) + 4;
			arm_dcache_log2_nsets =
			    31 - __builtin_clz(info->dcache_sets*2-1);
		}
	}

	info->cache_unified = (clidr == 4);

	if (level > 0) {
		info->dcache_type = CACHE_TYPE_PIPT;
		info->icache_type = CACHE_TYPE_PIPT;
	}

	if (info->cache_unified) {
		info->icache_ways = info->dcache_ways;
		info->icache_line_size = info->dcache_line_size;
		info->icache_way_size = info->dcache_way_size;
		info->icache_size = info->dcache_size;
	} else {
		csid = get_cachesize_cp15((level << 1)|CPU_CSSR_InD); /* select icache values */
		info->icache_sets = CPU_CSID_NUMSETS(csid) + 1;
		info->icache_ways = CPU_CSID_ASSOC(csid) + 1;
		info->icache_line_size = 1U << (CPU_CSID_LEN(csid) + 4);
		info->icache_way_size = info->icache_line_size * info->icache_sets;
		info->icache_size = info->icache_way_size * info->icache_ways;
	}
	if (level == 0
	    && info->dcache_way_size <= PAGE_SIZE
	    && info->icache_way_size <= PAGE_SIZE) {
		arm_cache_prefer_mask = 0;
	}
}
#endif /* (ARM_MMU_V6 + ARM_MMU_V7) > 0 */

static void
get_cachetype_cp15(void)
{
	u_int ctype, isize, dsize;
	u_int multiplier;

	ctype = armreg_ctr_read();

	/*
	 * ...and thus spake the ARM ARM:
	 *
	 * If an <opcode2> value corresponding to an unimplemented or
	 * reserved ID register is encountered, the System Control
	 * processor returns the value of the main ID register.
	 */
	if (ctype == cpu_id())
		goto out;

#if (ARM_MMU_V6 + ARM_MMU_V7) > 0
	if (CPU_CT_FORMAT(ctype) == 4) {
		u_int clidr = armreg_clidr_read();

		if (CPU_CT4_L1IPOLICY(ctype) == CPU_CT4_L1_PIPT) {
			arm_pcache.icache_type = CACHE_TYPE_PIPT;
		} else {
			arm_pcache.icache_type = CACHE_TYPE_VIPT;
			arm_cache_prefer_mask = PAGE_SIZE;
		}
#ifdef CPU_CORTEX
		if (CPU_ID_CORTEX_P(cpu_id())) {
			arm_pcache.dcache_type = CACHE_TYPE_PIPT;
		} else
#endif
		{
			arm_pcache.dcache_type = CACHE_TYPE_VIPT;
		}
		arm_pcache.cache_type = CPU_CT_CTYPE_WB14;

		get_cacheinfo_clidr(&arm_pcache, 0, clidr & 7);
		arm_dcache_align = arm_pcache.dcache_line_size;
		clidr >>= 3;
		if (clidr & 7) {
			get_cacheinfo_clidr(&arm_scache, 1, clidr & 7);
			if (arm_scache.dcache_line_size < arm_dcache_align)
				arm_dcache_align = arm_scache.dcache_line_size;
		}
		/*
		 * The pmap cleans an entire way for an exec page so
		 * we don't care that it's VIPT anymore.
		 */
		if (arm_pcache.dcache_type == CACHE_TYPE_PIPT) {
			arm_cache_prefer_mask = 0;
		}
		goto out;
	}
#endif /* ARM_MMU_V6 + ARM_MMU_V7 > 0 */

	if ((ctype & CPU_CT_S) == 0)
		arm_pcache.cache_unified = 1;

	/*
	 * If you want to know how this code works, go read the ARM ARM.
	 */

	arm_pcache.cache_type = CPU_CT_CTYPE(ctype);

	if (arm_pcache.cache_unified == 0) {
		isize = CPU_CT_ISIZE(ctype);
		multiplier = (isize & CPU_CT_xSIZE_M) ? 3 : 2;
		arm_pcache.icache_line_size = 1U << (CPU_CT_xSIZE_LEN(isize) + 3);
		if (CPU_CT_xSIZE_ASSOC(isize) == 0) {
			if (isize & CPU_CT_xSIZE_M)
				arm_pcache.icache_line_size = 0; /* not present */
			else
				arm_pcache.icache_ways = 1;
		} else {
			arm_pcache.icache_ways = multiplier <<
			    (CPU_CT_xSIZE_ASSOC(isize) - 1);
#if (ARM_MMU_V6 + ARM_MMU_V7) > 0
			arm_pcache.icache_type = CACHE_TYPE_VIPT;
			if (CPU_CT_xSIZE_P & isize)
				arm_cache_prefer_mask |=
				    __BIT(9 + CPU_CT_xSIZE_SIZE(isize)
					  - CPU_CT_xSIZE_ASSOC(isize))
				    - PAGE_SIZE;
#endif
		}
		arm_pcache.icache_size = multiplier << (CPU_CT_xSIZE_SIZE(isize) + 8);
		arm_pcache.icache_way_size =
		    __BIT(9 + CPU_CT_xSIZE_SIZE(isize) - CPU_CT_xSIZE_ASSOC(isize));
	}

	dsize = CPU_CT_DSIZE(ctype);
	multiplier = (dsize & CPU_CT_xSIZE_M) ? 3 : 2;
	arm_pcache.dcache_line_size = 1U << (CPU_CT_xSIZE_LEN(dsize) + 3);
	if (CPU_CT_xSIZE_ASSOC(dsize) == 0) {
		if (dsize & CPU_CT_xSIZE_M)
			arm_pcache.dcache_line_size = 0; /* not present */
		else
			arm_pcache.dcache_ways = 1;
	} else {
		arm_pcache.dcache_ways = multiplier <<
		    (CPU_CT_xSIZE_ASSOC(dsize) - 1);
#if (ARM_MMU_V6) > 0
		arm_pcache.dcache_type = CACHE_TYPE_VIPT;
		if ((CPU_CT_xSIZE_P & dsize)
		    && CPU_ID_ARM11_P(curcpu()->ci_arm_cpuid)) {
			arm_cache_prefer_mask |=
			    __BIT(9 + CPU_CT_xSIZE_SIZE(dsize)
				  - CPU_CT_xSIZE_ASSOC(dsize)) - PAGE_SIZE;
		}
#endif
	}
	arm_pcache.dcache_size = multiplier << (CPU_CT_xSIZE_SIZE(dsize) + 8);
	arm_pcache.dcache_way_size =
	    __BIT(9 + CPU_CT_xSIZE_SIZE(dsize) - CPU_CT_xSIZE_ASSOC(dsize));

	arm_dcache_align = arm_pcache.dcache_line_size;

	arm_dcache_log2_assoc = CPU_CT_xSIZE_ASSOC(dsize) + multiplier - 2;
	arm_dcache_log2_linesize = CPU_CT_xSIZE_LEN(dsize) + 3;
	arm_dcache_log2_nsets = 6 + CPU_CT_xSIZE_SIZE(dsize) -
	    CPU_CT_xSIZE_ASSOC(dsize) - CPU_CT_xSIZE_LEN(dsize);

 out:
	KASSERTMSG(arm_dcache_align <= CACHE_LINE_SIZE,
	    "arm_dcache_align=%u CACHE_LINE_SIZE=%u",
	    arm_dcache_align, CACHE_LINE_SIZE);
	arm_dcache_align_mask = arm_dcache_align - 1;
}
#endif /* ARM7TDMI || ARM8 || ARM9 || XSCALE */

#if defined(CPU_ARM2) || defined(CPU_ARM250) || defined(CPU_ARM3) || \
    defined(CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_SA110) || \
    defined(CPU_SA1100) || defined(CPU_SA1110) || defined(CPU_IXP12X0)
/* Cache information for CPUs without cache type registers. */
struct cachetab {
	uint32_t ct_cpuid;
	int	ct_pcache_type;
	int	ct_pcache_unified;
	int	ct_pdcache_size;
	int	ct_pdcache_line_size;
	int	ct_pdcache_ways;
	int	ct_picache_size;
	int	ct_picache_line_size;
	int	ct_picache_ways;
};

struct cachetab cachetab[] = {
    /* cpuid,           cache type,       u,  dsiz, ls, wy,  isiz, ls, wy */
    { CPU_ID_ARM2,      0,                1,     0,  0,  0,     0,  0,  0 },
    { CPU_ID_ARM250,    0,                1,     0,  0,  0,     0,  0,  0 },
    { CPU_ID_ARM3,      CPU_CT_CTYPE_WT,  1,  4096, 16, 64,     0,  0,  0 },
    { CPU_ID_ARM610,	CPU_CT_CTYPE_WT,  1,  4096, 16, 64,     0,  0,  0 },
    { CPU_ID_ARM710,    CPU_CT_CTYPE_WT,  1,  8192, 32,  4,     0,  0,  0 },
    { CPU_ID_ARM7500,   CPU_CT_CTYPE_WT,  1,  4096, 16,  4,     0,  0,  0 },
    { CPU_ID_ARM710A,   CPU_CT_CTYPE_WT,  1,  8192, 16,  4,     0,  0,  0 },
    { CPU_ID_ARM7500FE, CPU_CT_CTYPE_WT,  1,  4096, 16,  4,     0,  0,  0 },
    /* XXX is this type right for SA-1? */
    { CPU_ID_SA110,	CPU_CT_CTYPE_WB1, 0, 16384, 32, 32, 16384, 32, 32 },
    { CPU_ID_SA1100,	CPU_CT_CTYPE_WB1, 0,  8192, 32, 32, 16384, 32, 32 },
    { CPU_ID_SA1110,	CPU_CT_CTYPE_WB1, 0,  8192, 32, 32, 16384, 32, 32 },
    { CPU_ID_IXP1200,	CPU_CT_CTYPE_WB1, 0, 16384, 32, 32, 16384, 32, 32 }, /* XXX */
    { 0, 0, 0, 0, 0, 0, 0, 0}
};

static void get_cachetype_table(void);

static void
get_cachetype_table(void)
{
	int i;
	uint32_t cpuid = cpu_id();

	for (i = 0; cachetab[i].ct_cpuid != 0; i++) {
		if (cachetab[i].ct_cpuid == (cpuid & CPU_ID_CPU_MASK)) {
			arm_pcache.cache_type = cachetab[i].ct_pcache_type;
			arm_pcache.cache_unified = cachetab[i].ct_pcache_unified;
			arm_pcache.dcache_size = cachetab[i].ct_pdcache_size;
			arm_pcache.dcache_line_size =
			    cachetab[i].ct_pdcache_line_size;
			arm_pcache.dcache_ways = cachetab[i].ct_pdcache_ways;
			if (arm_pcache.dcache_ways) {
				arm_pcache.dcache_way_size =
				    arm_pcache.dcache_line_size
				    / arm_pcache.dcache_ways;
			}
			arm_pcache.icache_size = cachetab[i].ct_picache_size;
			arm_pcache.icache_line_size =
			    cachetab[i].ct_picache_line_size;
			arm_pcache.icache_ways = cachetab[i].ct_picache_ways;
			if (arm_pcache.icache_ways) {
				arm_pcache.icache_way_size =
				    arm_pcache.icache_line_size
				    / arm_pcache.icache_ways;
			}
		}
	}

	arm_dcache_align = arm_pcache.dcache_line_size;
	arm_dcache_align_mask = arm_dcache_align - 1;
}

#endif /* ARM2 || ARM250 || ARM3 || ARM6 || ARM7 || SA110 || SA1100 || SA1111 || IXP12X0 */

/*
 * Cannot panic here as we may not have a console yet ...
 */

int
set_cpufuncs(void)
{
	if (cputype == 0) {
		cputype = cpufunc_id();
		cputype &= CPU_ID_CPU_MASK;
	}

	/*
	 * NOTE: cpu_do_powersave defaults to off.  If we encounter a
	 * CPU type where we want to use it by default, then we set it.
	 */
#ifdef CPU_ARM2
	if (cputype == CPU_ID_ARM2) {
		cpufuncs = arm2_cpufuncs;
		get_cachetype_table();
		return 0;
	}
#endif /* CPU_ARM2 */
#ifdef CPU_ARM250
	if (cputype == CPU_ID_ARM250) {
		cpufuncs = arm250_cpufuncs;
		get_cachetype_table();
		return 0;
	}
#endif
#ifdef CPU_ARM3
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    (cputype & 0x00000f00) == 0x00000300) {
		cpufuncs = arm3_cpufuncs;
		get_cachetype_table();
		return 0;
	}
#endif	/* CPU_ARM3 */
#ifdef CPU_ARM6
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    (cputype & 0x00000f00) == 0x00000600) {
		cpufuncs = arm6_cpufuncs;
		get_cachetype_table();
		pmap_pte_init_generic();
		return 0;
	}
#endif	/* CPU_ARM6 */
#ifdef CPU_ARM7
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    CPU_ID_IS7(cputype) &&
	    (cputype & CPU_ID_7ARCH_MASK) == CPU_ID_7ARCH_V3) {
		cpufuncs = arm7_cpufuncs;
		get_cachetype_table();
		pmap_pte_init_generic();
		return 0;
	}
#endif	/* CPU_ARM7 */
#ifdef CPU_ARM7TDMI
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    CPU_ID_IS7(cputype) &&
	    (cputype & CPU_ID_7ARCH_MASK) == CPU_ID_7ARCH_V4T) {
		cpufuncs = arm7tdmi_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_generic();
		return 0;
	}
#endif
#ifdef CPU_ARM8
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    (cputype & 0x0000f000) == 0x00008000) {
		cpufuncs = arm8_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_arm8();
		return 0;
	}
#endif	/* CPU_ARM8 */
#ifdef CPU_ARM9
	if (((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD ||
	     (cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_TI) &&
	    (cputype & 0x0000f000) == 0x00009000) {
		cpufuncs = arm9_cpufuncs;
		get_cachetype_cp15();
		arm9_dcache_sets_inc = 1U << arm_dcache_log2_linesize;
		arm9_dcache_sets_max =
		    (1U << (arm_dcache_log2_linesize + arm_dcache_log2_nsets)) -
		    arm9_dcache_sets_inc;
		arm9_dcache_index_inc = 1U << (32 - arm_dcache_log2_assoc);
		arm9_dcache_index_max = 0U - arm9_dcache_index_inc;
#ifdef	ARM9_CACHE_WRITE_THROUGH
		pmap_pte_init_arm9();
#else
		pmap_pte_init_generic();
#endif
		return 0;
	}
#endif /* CPU_ARM9 */
#if defined(CPU_ARM9E) || defined(CPU_ARM10)
	if (cputype == CPU_ID_ARM926EJS ||
	    cputype == CPU_ID_ARM1026EJS) {
		cpufuncs = armv5_ec_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_generic();
		return 0;
	}
#endif /* CPU_ARM9E || CPU_ARM10 */
#if defined(CPU_SHEEVA)
	if (cputype == CPU_ID_MV88SV131 ||
	    cputype == CPU_ID_MV88FR571_VD) {
		cpufuncs = sheeva_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_generic();
		cpu_do_powersave = 1;			/* Enable powersave */
		return 0;
	}
#endif /* CPU_SHEEVA */
#ifdef CPU_ARM10
	if (/* cputype == CPU_ID_ARM1020T || */
	    cputype == CPU_ID_ARM1020E) {
		/*
		 * Select write-through cacheing (this isn't really an
		 * option on ARM1020T).
		 */
		cpufuncs = arm10_cpufuncs;
		get_cachetype_cp15();
		armv5_dcache_sets_inc = 1U << arm_dcache_log2_linesize;
		armv5_dcache_sets_max =
		    (1U << (arm_dcache_log2_linesize + arm_dcache_log2_nsets)) -
		    armv5_dcache_sets_inc;
		armv5_dcache_index_inc = 1U << (32 - arm_dcache_log2_assoc);
		armv5_dcache_index_max = 0U - armv5_dcache_index_inc;
		pmap_pte_init_generic();
		return 0;
	}
#endif /* CPU_ARM10 */


#if defined(CPU_ARM11MPCORE)
	if (cputype == CPU_ID_ARM11MPCORE) {
		cpufuncs = arm11mpcore_cpufuncs;
#if defined(CPU_ARMV7) || defined(CPU_PRE_ARMV6)
		cpu_armv6_p = true;
#endif
		get_cachetype_cp15();
		armv5_dcache_sets_inc = 1U << arm_dcache_log2_linesize;
		armv5_dcache_sets_max = (1U << (arm_dcache_log2_linesize +
			arm_dcache_log2_nsets)) - armv5_dcache_sets_inc;
		armv5_dcache_index_inc = 1U << (32 - arm_dcache_log2_assoc);
		armv5_dcache_index_max = 0U - armv5_dcache_index_inc;
		cpu_do_powersave = 1;			/* Enable powersave */
		pmap_pte_init_arm11mpcore();
		if (arm_cache_prefer_mask)
			uvmexp.ncolors = (arm_cache_prefer_mask >> PGSHIFT) + 1;

		return 0;

	}
#endif	/* CPU_ARM11MPCORE */

#if defined(CPU_ARM11)
	if (cputype == CPU_ID_ARM1136JS ||
	    cputype == CPU_ID_ARM1136JSR1 ||
	    cputype == CPU_ID_ARM1176JZS) {
		cpufuncs = arm11_cpufuncs;
#if defined(CPU_ARM1136)
		if (cputype == CPU_ID_ARM1136JS &&
		    cputype == CPU_ID_ARM1136JSR1) {
			cpufuncs = arm1136_cpufuncs;
			if (cputype == CPU_ID_ARM1136JS)
				cpufuncs.cf_sleep = arm1136_sleep_rev0;
		}
#endif
#if defined(CPU_ARM1176)
		if (cputype == CPU_ID_ARM1176JZS) {
			cpufuncs = arm1176_cpufuncs;
		}
#endif
#if defined(CPU_ARMV7) || defined(CPU_PRE_ARMV6)
		cpu_armv6_p = true;
#endif
		cpu_do_powersave = 1;			/* Enable powersave */
		get_cachetype_cp15();
#ifdef ARM11_CACHE_WRITE_THROUGH
		pmap_pte_init_arm11();
#else
		pmap_pte_init_generic();
#endif
		if (arm_cache_prefer_mask)
			uvmexp.ncolors = (arm_cache_prefer_mask >> PGSHIFT) + 1;

		/*
		 * Start and reset the PMC Cycle Counter.
		 */
		armreg_pmcrv6_write(ARM11_PMCCTL_E | ARM11_PMCCTL_P | ARM11_PMCCTL_C);
		return 0;
	}
#endif /* CPU_ARM11 */
#ifdef CPU_SA110
	if (cputype == CPU_ID_SA110) {
		cpufuncs = sa110_cpufuncs;
		get_cachetype_table();
		pmap_pte_init_sa1();
		return 0;
	}
#endif	/* CPU_SA110 */
#ifdef CPU_SA1100
	if (cputype == CPU_ID_SA1100) {
		cpufuncs = sa11x0_cpufuncs;
		get_cachetype_table();
		pmap_pte_init_sa1();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;

		return 0;
	}
#endif	/* CPU_SA1100 */
#ifdef CPU_SA1110
	if (cputype == CPU_ID_SA1110) {
		cpufuncs = sa11x0_cpufuncs;
		get_cachetype_table();
		pmap_pte_init_sa1();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;

		return 0;
	}
#endif	/* CPU_SA1110 */
#ifdef CPU_FA526
	if (cputype == CPU_ID_FA526) {
		cpufuncs = fa526_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_generic();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;

		return 0;
	}
#endif	/* CPU_FA526 */
#ifdef CPU_IXP12X0
	if (cputype == CPU_ID_IXP1200) {
		cpufuncs = ixp12x0_cpufuncs;
		get_cachetype_table();
		pmap_pte_init_sa1();
		return 0;
	}
#endif  /* CPU_IXP12X0 */
#ifdef CPU_XSCALE_80200
	if (cputype == CPU_ID_80200) {
		int rev = cpufunc_id() & CPU_ID_REVISION_MASK;

		i80200_icu_init();

		/*
		 * Reset the Performance Monitoring Unit to a
		 * pristine state:
		 *	- CCNT, PMN0, PMN1 reset to 0
		 *	- overflow indications cleared
		 *	- all counters disabled
		 */
		__asm volatile("mcr p14, 0, %0, c0, c0, 0"
			:
			: "r" (PMNC_P|PMNC_C|PMNC_PMN0_IF|PMNC_PMN1_IF|
			       PMNC_CC_IF));

#if defined(XSCALE_CCLKCFG)
		/*
		 * Crank CCLKCFG to maximum legal value.
		 */
		__asm volatile ("mcr p14, 0, %0, c6, c0, 0"
			:
			: "r" (XSCALE_CCLKCFG));
#endif

		/*
		 * XXX Disable ECC in the Bus Controller Unit; we
		 * don't really support it, yet.  Clear any pending
		 * error indications.
		 */
		__asm volatile("mcr p13, 0, %0, c0, c1, 0"
			:
			: "r" (BCUCTL_E0|BCUCTL_E1|BCUCTL_EV));

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		/*
		 * i80200 errata: Step-A0 and A1 have a bug where
		 * D$ dirty bits are not cleared on "invalidate by
		 * address".
		 *
		 * Workaround: Clean cache line before invalidating.
		 */
		if (rev == 0 || rev == 1)
			cpufuncs.cf_dcache_inv_range = xscale_cache_purgeD_rng;

		get_cachetype_cp15();
		pmap_pte_init_xscale();
		return 0;
	}
#endif /* CPU_XSCALE_80200 */
#ifdef CPU_XSCALE_80321
	if (cputype == CPU_ID_80321_400 || cputype == CPU_ID_80321_600 ||
	    cputype == CPU_ID_80321_400_B0 || cputype == CPU_ID_80321_600_B0 ||
	    cputype == CPU_ID_80219_400 || cputype == CPU_ID_80219_600) {
		i80321_icu_init();

		/*
		 * Reset the Performance Monitoring Unit to a
		 * pristine state:
		 *	- CCNT, PMN0, PMN1 reset to 0
		 *	- overflow indications cleared
		 *	- all counters disabled
		 */
		__asm volatile("mcr p14, 0, %0, c0, c0, 0"
			:
			: "r" (PMNC_P|PMNC_C|PMNC_PMN0_IF|PMNC_PMN1_IF|
			       PMNC_CC_IF));

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		get_cachetype_cp15();
		pmap_pte_init_xscale();
		return 0;
	}
#endif /* CPU_XSCALE_80321 */
#ifdef __CPU_XSCALE_PXA2XX
	/* ignore core revision to test PXA2xx CPUs */
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X ||
	    (cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA250 ||
	    (cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA210) {

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		get_cachetype_cp15();
		pmap_pte_init_xscale();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;

		return 0;
	}
#endif /* __CPU_XSCALE_PXA2XX */
#ifdef CPU_XSCALE_IXP425
	if (cputype == CPU_ID_IXP425_533 || cputype == CPU_ID_IXP425_400 ||
	    cputype == CPU_ID_IXP425_266) {
		ixp425_icu_init();

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		get_cachetype_cp15();
		pmap_pte_init_xscale();

		return 0;
	}
#endif /* CPU_XSCALE_IXP425 */
#if defined(CPU_CORTEX)
	if (CPU_ID_CORTEX_P(cputype)) {
		cpufuncs = armv7_cpufuncs;
		cpu_do_powersave = 1;			/* Enable powersave */
#if defined(CPU_ARMV6) || defined(CPU_PRE_ARMV6)
		cpu_armv7_p = true;
#endif
		get_cachetype_cp15();
		pmap_pte_init_armv7();
		if (arm_cache_prefer_mask)
			uvmexp.ncolors = (arm_cache_prefer_mask >> PGSHIFT) + 1;
		/*
		 * Start and reset the PMC Cycle Counter.
		 */
		armreg_pmcr_write(ARM11_PMCCTL_E | ARM11_PMCCTL_P | ARM11_PMCCTL_C);
		armreg_pmcntenset_write(CORTEX_CNTENS_C);
		return 0;
	}
#endif /* CPU_CORTEX */

#if defined(CPU_PJ4B)
	if ((cputype == CPU_ID_MV88SV581X_V6 ||
	    cputype == CPU_ID_MV88SV581X_V7 ||
	    cputype == CPU_ID_MV88SV584X_V7 ||
	    cputype == CPU_ID_ARM_88SV581X_V6 ||
	    cputype == CPU_ID_ARM_88SV581X_V7) &&
	    (armreg_pfr0_read() & ARM_PFR0_THUMBEE_MASK)) {
			cpufuncs = pj4bv7_cpufuncs;
#if defined(CPU_ARMV6) || defined(CPU_PRE_ARMV6)
			cpu_armv7_p = true;
#endif
			get_cachetype_cp15();
			pmap_pte_init_armv7();
			return 0;
	}
#endif /* CPU_PJ4B */

	/*
	 * Bzzzz. And the answer was ...
	 */
	panic("No support for this CPU type (%08x) in kernel", cputype);
	return(ARCHITECTURE_NOT_PRESENT);
}

#ifdef CPU_ARM2
u_int arm2_id(void)
{

	return CPU_ID_ARM2;
}
#endif /* CPU_ARM2 */

#ifdef CPU_ARM250
u_int arm250_id(void)
{

	return CPU_ID_ARM250;
}
#endif /* CPU_ARM250 */

/*
 * Fixup routines for data and prefetch aborts.
 *
 * Several compile time symbols are used
 *
 * DEBUG_FAULT_CORRECTION - Print debugging information during the
 * correction of registers after a fault.
 * ARM6_LATE_ABORT - ARM6 supports both early and late aborts
 * when defined should use late aborts
 */


/*
 * Null abort fixup routine.
 * For use when no fixup is required.
 */
int
cpufunc_null_fixup(void *arg)
{
	return(ABORT_FIXUP_OK);
}


#if defined(CPU_ARM2) || defined(CPU_ARM250) || defined(CPU_ARM3) || \
    defined(CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI)

#ifdef DEBUG_FAULT_CORRECTION
#define DFC_PRINTF(x)		printf x
#define DFC_DISASSEMBLE(x)	disassemble(x)
#else
#define DFC_PRINTF(x)		/* nothing */
#define DFC_DISASSEMBLE(x)	/* nothing */
#endif

/*
 * "Early" data abort fixup.
 *
 * For ARM2, ARM2as, ARM3 and ARM6 (in early-abort mode).  Also used
 * indirectly by ARM6 (in late-abort mode) and ARM7[TDMI].
 *
 * In early aborts, we may have to fix up LDM, STM, LDC and STC.
 */
int
early_abort_fixup(void *arg)
{
	trapframe_t *frame = arg;
	u_int fault_pc;
	u_int fault_instruction;
	int saved_lr = 0;

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the following fixup code.
		 */

		saved_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = frame->tf_svc_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	/* Get fault address and status from the CPU */

	fault_pc = frame->tf_pc;
	fault_instruction = *((volatile unsigned int *)fault_pc);

	/* Decode the fault instruction and fix the registers as needed */

	if ((fault_instruction & 0x0e000000) == 0x08000000) {
		int base;
		int loop;
		int count;
		int *registers = &frame->tf_r0;

		DFC_PRINTF(("LDM/STM\n"));
		DFC_DISASSEMBLE(fault_pc);
		if (fault_instruction & (1 << 21)) {
			DFC_PRINTF(("This instruction must be corrected\n"));
			base = (fault_instruction >> 16) & 0x0f;
			if (base == 15)
				return ABORT_FIXUP_FAILED;
			/* Count registers transferred */
			count = 0;
			for (loop = 0; loop < 16; ++loop) {
				if (fault_instruction & (1<<loop))
					++count;
			}
			DFC_PRINTF(("%d registers used\n", count));
			DFC_PRINTF(("Corrected r%d by %d bytes ",
				       base, count * 4));
			if (fault_instruction & (1 << 23)) {
				DFC_PRINTF(("down\n"));
				registers[base] -= count * 4;
			} else {
				DFC_PRINTF(("up\n"));
				registers[base] += count * 4;
			}
		}
	} else if ((fault_instruction & 0x0e000000) == 0x0c000000) {
		int base;
		int offset;
		int *registers = &frame->tf_r0;

		/* REGISTER CORRECTION IS REQUIRED FOR THESE INSTRUCTIONS */

		DFC_DISASSEMBLE(fault_pc);

		/* Only need to fix registers if write back is turned on */

		if ((fault_instruction & (1 << 21)) != 0) {
			base = (fault_instruction >> 16) & 0x0f;
			if (base == 13 &&
			    (frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE)
				return ABORT_FIXUP_FAILED;
			if (base == 15)
				return ABORT_FIXUP_FAILED;

			offset = (fault_instruction & 0xff) << 2;
			DFC_PRINTF(("r%d=%08x\n", base, registers[base]));
			if ((fault_instruction & (1 << 23)) != 0)
				offset = -offset;
			registers[base] += offset;
			DFC_PRINTF(("r%d=%08x\n", base, registers[base]));
		}
	} else if ((fault_instruction & 0x0e000000) == 0x0c000000)
		return ABORT_FIXUP_FAILED;

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the prior fixup code.
		 */

		frame->tf_svc_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = saved_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	return(ABORT_FIXUP_OK);
}
#endif	/* CPU_ARM2/250/3/6/7 */


#if (defined(CPU_ARM6) && defined(ARM6_LATE_ABORT)) || defined(CPU_ARM7) || \
	defined(CPU_ARM7TDMI)
/*
 * "Late" (base updated) data abort fixup
 *
 * For ARM6 (in late-abort mode) and ARM7.
 *
 * In this model, all data-transfer instructions need fixing up.  We defer
 * LDM, STM, LDC and STC fixup to the early-abort handler.
 */
int
late_abort_fixup(void *arg)
{
	trapframe_t *frame = arg;
	u_int fault_pc;
	u_int fault_instruction;
	int saved_lr = 0;

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the following fixup code.
		 */

		saved_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = frame->tf_svc_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	/* Get fault address and status from the CPU */

	fault_pc = frame->tf_pc;
	fault_instruction = *((volatile unsigned int *)fault_pc);

	/* Decode the fault instruction and fix the registers as needed */

	/* Was is a swap instruction ? */

	if ((fault_instruction & 0x0fb00ff0) == 0x01000090) {
		DFC_DISASSEMBLE(fault_pc);
	} else if ((fault_instruction & 0x0c000000) == 0x04000000) {

		/* Was is a ldr/str instruction */
		/* This is for late abort only */

		int base;
		int offset;
		int *registers = &frame->tf_r0;

		DFC_DISASSEMBLE(fault_pc);

		/* This is for late abort only */

		if ((fault_instruction & (1 << 24)) == 0
		    || (fault_instruction & (1 << 21)) != 0) {
			/* postindexed ldr/str with no writeback */

			base = (fault_instruction >> 16) & 0x0f;
			if (base == 13 &&
			    (frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE)
				return ABORT_FIXUP_FAILED;
			if (base == 15)
				return ABORT_FIXUP_FAILED;
			DFC_PRINTF(("late abt fix: r%d=%08x : ",
				       base, registers[base]));
			if ((fault_instruction & (1 << 25)) == 0) {
				/* Immediate offset - easy */

				offset = fault_instruction & 0xfff;
				if ((fault_instruction & (1 << 23)))
					offset = -offset;
				registers[base] += offset;
				DFC_PRINTF(("imm=%08x ", offset));
			} else {
				/* offset is a shifted register */
				int shift;

				offset = fault_instruction & 0x0f;
				if (offset == base)
					return ABORT_FIXUP_FAILED;

				/*
				 * Register offset - hard we have to
				 * cope with shifts !
				 */
				offset = registers[offset];

				if ((fault_instruction & (1 << 4)) == 0)
					/* shift with amount */
					shift = (fault_instruction >> 7) & 0x1f;
				else {
					/* shift with register */
					if ((fault_instruction & (1 << 7)) != 0)
						/* undefined for now so bail out */
						return ABORT_FIXUP_FAILED;
					shift = ((fault_instruction >> 8) & 0xf);
					if (base == shift)
						return ABORT_FIXUP_FAILED;
					DFC_PRINTF(("shift reg=%d ", shift));
					shift = registers[shift];
				}
				DFC_PRINTF(("shift=%08x ", shift));
				switch (((fault_instruction >> 5) & 0x3)) {
				case 0 : /* Logical left */
					offset = (int)(((u_int)offset) << shift);
					break;
				case 1 : /* Logical Right */
					if (shift == 0) shift = 32;
					offset = (int)(((u_int)offset) >> shift);
					break;
				case 2 : /* Arithmetic Right */
					if (shift == 0) shift = 32;
					offset = (int)(((int)offset) >> shift);
					break;
				case 3 : /* Rotate right (rol or rxx) */
					return ABORT_FIXUP_FAILED;
					break;
				}

				DFC_PRINTF(("abt: fixed LDR/STR with "
					       "register offset\n"));
				if ((fault_instruction & (1 << 23)))
					offset = -offset;
				DFC_PRINTF(("offset=%08x ", offset));
				registers[base] += offset;
			}
			DFC_PRINTF(("r%d=%08x\n", base, registers[base]));
		}
	}

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the prior fixup code.
		 */

		frame->tf_svc_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = saved_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	/*
	 * Now let the early-abort fixup routine have a go, in case it
	 * was an LDM, STM, LDC or STC that faulted.
	 */

	return early_abort_fixup(arg);
}
#endif	/* CPU_ARM6(LATE)/7/7TDMI */

/*
 * CPU Setup code
 */

#if defined(CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI) || \
	defined(CPU_ARM8) || defined (CPU_ARM9) || defined (CPU_ARM9E) || \
	defined(CPU_SA110) || defined(CPU_SA1100) || defined(CPU_SA1110) || \
	defined(CPU_FA526) || \
	defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
	defined(__CPU_XSCALE_PXA2XX) || defined(CPU_XSCALE_IXP425) || \
	defined(CPU_ARM10) || defined(CPU_SHEEVA) || \
	defined(CPU_ARMV6) || defined(CPU_ARMV7)

#define IGN	0
#define OR	1
#define BIC	2

struct cpu_option {
	const char *co_name;
	int	co_falseop;
	int	co_trueop;
	int	co_value;
};

static u_int parse_cpu_options(char *, struct cpu_option *, u_int);

static u_int
parse_cpu_options(char *args, struct cpu_option *optlist, u_int cpuctrl)
{
	int integer;

	if (args == NULL)
		return(cpuctrl);

	while (optlist->co_name) {
		if (get_bootconf_option(args, optlist->co_name,
		    BOOTOPT_TYPE_BOOLEAN, &integer)) {
			if (integer) {
				if (optlist->co_trueop == OR)
					cpuctrl |= optlist->co_value;
				else if (optlist->co_trueop == BIC)
					cpuctrl &= ~optlist->co_value;
			} else {
				if (optlist->co_falseop == OR)
					cpuctrl |= optlist->co_value;
				else if (optlist->co_falseop == BIC)
					cpuctrl &= ~optlist->co_value;
			}
		}
		++optlist;
	}
	return(cpuctrl);
}
#endif /* CPU_ARM6 || CPU_ARM7 || CPU_ARM7TDMI || CPU_ARM8 || CPU_SA110 */

#if defined (CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI) \
	|| defined(CPU_ARM8)
struct cpu_option arm678_options[] = {
#ifdef COMPAT_12
	{ "nocache",		IGN, BIC, CPU_CONTROL_IDC_ENABLE },
	{ "nowritebuf",		IGN, BIC, CPU_CONTROL_WBUF_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "cpu.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

#endif	/* CPU_ARM6 || CPU_ARM7 || CPU_ARM7TDMI || CPU_ARM8 */

#ifdef CPU_ARM6
struct cpu_option arm6_options[] = {
	{ "arm6.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm6.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm6.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm6.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm6_setup(char *args)
{

	/* Set up default control registers bits */
	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_BEND_ENABLE
		 | CPU_CONTROL_AFLT_ENABLE;
#endif

#ifdef ARM6_LATE_ABORT
	cpuctrl |= CPU_CONTROL_LABT_ENABLE;
#endif	/* ARM6_LATE_ABORT */

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm6_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_ARM6 */

#ifdef CPU_ARM7
struct cpu_option arm7_options[] = {
	{ "arm7.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm7.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm7.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm7.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
#ifdef COMPAT_12
	{ "fpaclk2",		BIC, OR,  CPU_CONTROL_CPCLK },
#endif	/* COMPAT_12 */
	{ "arm700.fpaclk",	BIC, OR,  CPU_CONTROL_CPCLK },
	{ NULL,			IGN, IGN, 0 }
};

void
arm7_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_LABT_ENABLE
		 | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_BEND_ENABLE
		 | CPU_CONTROL_AFLT_ENABLE;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm7_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_ARM7 */

#ifdef CPU_ARM7TDMI
struct cpu_option arm7tdmi_options[] = {
	{ "arm7.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm7.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm7.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm7.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
#ifdef COMPAT_12
	{ "fpaclk2",		BIC, OR,  CPU_CONTROL_CPCLK },
#endif	/* COMPAT_12 */
	{ "arm700.fpaclk",	BIC, OR,  CPU_CONTROL_CPCLK },
	{ NULL,			IGN, IGN, 0 }
};

void
arm7tdmi_setup(char *args)
{
	int cpuctrl;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm7tdmi_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_ARM7TDMI */

#ifdef CPU_ARM8
struct cpu_option arm8_options[] = {
	{ "arm8.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm8.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm8.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm8.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
#ifdef COMPAT_12
	{ "branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "arm8.branchpredict",	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm8_setup(char *args)
{
	int integer;
	int clocktest;
	int setclock = 0;

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_BPRD_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm8_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Get clock configuration */
	clocktest = arm8_clock_config(0, 0) & 0x0f;

	/* Special ARM8 clock and test configuration */
	if (get_bootconf_option(args, "arm8.clock.reset", BOOTOPT_TYPE_BOOLEAN, &integer)) {
		clocktest = 0;
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.clock.dynamic", BOOTOPT_TYPE_BOOLEAN, &integer)) {
		if (integer)
			clocktest |= 0x01;
		else
			clocktest &= ~(0x01);
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.clock.sync", BOOTOPT_TYPE_BOOLEAN, &integer)) {
		if (integer)
			clocktest |= 0x02;
		else
			clocktest &= ~(0x02);
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.clock.fast", BOOTOPT_TYPE_BININT, &integer)) {
		clocktest = (clocktest & ~0xc0) | (integer & 3) << 2;
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.test", BOOTOPT_TYPE_BININT, &integer)) {
		clocktest |= (integer & 7) << 5;
		setclock = 1;
	}

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* Set the clock/test register */
	if (setclock)
		arm8_clock_config(0x7f, clocktest);
}
#endif	/* CPU_ARM8 */

#ifdef CPU_ARM9
struct cpu_option arm9_options[] = {
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm9.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm9.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "arm9.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "arm9.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm9_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
	    | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE;
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_VECRELOC
		 | CPU_CONTROL_ROUNDROBIN;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm9_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(cpuctrlmask, cpuctrl);

}
#endif	/* CPU_ARM9 */

#if defined(CPU_ARM9E) || defined(CPU_ARM10)
struct cpu_option arm10_options[] = {
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm10.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm10.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "arm10.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "arm10.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm10_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_BPRD_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
	    | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_ROUNDROBIN | CPU_CONTROL_CPCLK;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm10_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARM9E || CPU_ARM10 */

#if defined(CPU_ARM11)
struct cpu_option arm11_options[] = {
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm11.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm11.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "arm11.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "arm11.branchpredict", BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm11_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
#ifdef ARM_MMU_EXTENDED
	    | CPU_CONTROL_XP_ENABLE
#endif
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    /* | CPU_CONTROL_BPRD_ENABLE */;
	int cpuctrlmask = cpuctrl
	    | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_ROUNDROBIN | CPU_CONTROL_CPCLK;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm11_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );

	/* Allow detection code to find the VFP if it's fitted.  */
	armreg_cpacr_write(0x0fffffff);

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(cpuctrlmask, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARM11 */

#if defined(CPU_ARM11MPCORE)

void
arm11mpcore_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_IC_ENABLE
	    | CPU_CONTROL_DC_ENABLE
#ifdef ARM_MMU_EXTENDED
	    | CPU_CONTROL_XP_ENABLE
#endif
	    | CPU_CONTROL_BPRD_ENABLE ;
	int cpuctrlmask = cpuctrl
	    | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_VECRELOC;

#ifdef	ARM11MPCORE_MMU_COMPAT
	/* XXX: S and R? */
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm11_options, cpuctrl);

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );

	/* Allow detection code to find the VFP if it's fitted.  */
	armreg_cpacr_write(0x0fffffff);

	/* Set the control register */
	curcpu()->ci_ctrl = cpu_control(cpuctrlmask, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARM11MPCORE */

#ifdef CPU_PJ4B
void
pj4bv7_setup(char *args)
{
	int cpuctrl;

	pj4b_config();

	cpuctrl = CPU_CONTROL_MMU_ENABLE;
#ifdef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_UNAL_ENABLE;
#else
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif
	cpuctrl |= CPU_CONTROL_DC_ENABLE;
	cpuctrl |= CPU_CONTROL_IC_ENABLE;
	cpuctrl |= (0xf << 3);
	cpuctrl |= CPU_CONTROL_BPRD_ENABLE;
	cpuctrl |= (0x5 << 16) | (1 < 22);
	cpuctrl |= CPU_CONTROL_XP_ENABLE;

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

#ifdef L2CACHE_ENABLE
	/* Setup L2 cache */
	arm_scache.cache_type = CPU_CT_CTYPE_WT;
	arm_scache.cache_unified = 1;
	arm_scache.dcache_type = arm_scache.icache_type = CACHE_TYPE_PIPT;
	arm_scache.dcache_size = arm_scache.icache_size = ARMADAXP_L2_SIZE;
	arm_scache.dcache_ways = arm_scache.icache_ways = ARMADAXP_L2_WAYS;
	arm_scache.dcache_way_size = arm_scache.icache_way_size =
	    ARMADAXP_L2_WAY_SIZE;
	arm_scache.dcache_line_size = arm_scache.icache_line_size =
	    ARMADAXP_L2_LINE_SIZE;
	arm_scache.dcache_sets = arm_scache.icache_sets =
	    ARMADAXP_L2_SETS;

	cpufuncs.cf_sdcache_wbinv_range	= armadaxp_sdcache_wbinv_range;
	cpufuncs.cf_sdcache_inv_range	= armadaxp_sdcache_inv_range;
	cpufuncs.cf_sdcache_wb_range	= armadaxp_sdcache_wb_range;
#endif

#ifdef AURORA_IO_CACHE_COHERENCY
	/* use AMBA and I/O Coherency Fabric to maintain cache */
	cpufuncs.cf_dcache_wbinv_range	= pj4b_dcache_cfu_wbinv_range;
	cpufuncs.cf_dcache_inv_range	= pj4b_dcache_cfu_inv_range;
	cpufuncs.cf_dcache_wb_range	= pj4b_dcache_cfu_wb_range;

	cpufuncs.cf_sdcache_wbinv_range	= (void *)cpufunc_nullop;
	cpufuncs.cf_sdcache_inv_range	= (void *)cpufunc_nullop;
	cpufuncs.cf_sdcache_wb_range	= (void *)cpufunc_nullop;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	cpu_control(0xffffffff, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
#ifdef L2CACHE_ENABLE
	armadaxp_sdcache_wbinv_all();
#endif

	curcpu()->ci_ctrl = cpuctrl;
}
#endif /* CPU_PJ4B */

#if defined(CPU_ARMV7)
struct cpu_option armv7_options[] = {
    { "cpu.cache",      BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
    { "cpu.nocache",    OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
    { "armv7.cache",    BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
    { "armv7.icache",   BIC, OR,  CPU_CONTROL_IC_ENABLE },
    { "armv7.dcache",   BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ NULL, 			IGN, IGN, 0}
};

void
armv7_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_IC_ENABLE
	    | CPU_CONTROL_DC_ENABLE | CPU_CONTROL_BPRD_ENABLE
#ifdef __ARMEB__
	    | CPU_CONTROL_EX_BEND
#endif
#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	    | CPU_CONTROL_AFLT_ENABLE;
#endif
	    | CPU_CONTROL_UNAL_ENABLE;

	int cpuctrlmask = cpuctrl | CPU_CONTROL_AFLT_ENABLE;


	cpuctrl = parse_cpu_options(args, armv7_options, cpuctrl);

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(cpuctrlmask, cpuctrl);
}
#endif /* CPU_ARMV7 */


#if defined(CPU_ARM1136) || defined(CPU_ARM1176)
void
arm11x6_setup(char *args)
{
	int cpuctrl, cpuctrl_wax;
	uint32_t auxctrl;
	uint32_t sbz=0;
	uint32_t cpuid;

	cpuid = cpu_id();

	cpuctrl =
		CPU_CONTROL_MMU_ENABLE  |
		CPU_CONTROL_DC_ENABLE   |
		CPU_CONTROL_WBUF_ENABLE |
		CPU_CONTROL_32BP_ENABLE |
		CPU_CONTROL_32BD_ENABLE |
		CPU_CONTROL_LABT_ENABLE |
		CPU_CONTROL_UNAL_ENABLE |
#ifdef ARM_MMU_EXTENDED
		CPU_CONTROL_XP_ENABLE   |
#else
		CPU_CONTROL_SYST_ENABLE |
#endif
		CPU_CONTROL_IC_ENABLE;

	/*
	 * "write as existing" bits
	 * inverse of this is mask
	 */
	cpuctrl_wax =
		(3 << 30) |
		(1 << 29) |
		(1 << 28) |
		(3 << 26) |
		(3 << 19) |
		(1 << 17);

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, arm11_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	auxctrl = armreg_auxctl_read();
	/*
	 * This options enables the workaround for the 364296 ARM1136
	 * r0pX errata (possible cache data corruption with
	 * hit-under-miss enabled). It sets the undocumented bit 31 in
	 * the auxiliary control register and the FI bit in the control
	 * register, thus disabling hit-under-miss without putting the
	 * processor into full low interrupt latency mode. ARM11MPCore
	 * is not affected.
	 */
	if ((cpuid & CPU_ID_CPU_MASK) == CPU_ID_ARM1136JS) { /* ARM1136JSr0pX */
		cpuctrl |= CPU_CONTROL_FI_ENABLE;
		auxctrl |= ARM1136_AUXCTL_PFI;
	}

	/*
	 * This enables the workaround for the following ARM1176 r0pX
	 * errata.
	 *
	 * 394601: In low interrupt latency configuration, interrupted clean
	 * and invalidate operation may not clean dirty data.
	 *
	 * 716151: Clean Data Cache line by MVA can corrupt subsequent
	 * stores to the same cache line.
	 *
	 * 714068: Prefetch Instruction Cache Line or Invalidate Instruction
	 * Cache Line by MVA can cause deadlock.
	 */
	if ((cpuid & CPU_ID_CPU_MASK) == CPU_ID_ARM1176JZS) { /* ARM1176JZSr0 */
		/* 394601 and 716151 */
		cpuctrl |= CPU_CONTROL_FI_ENABLE;
		auxctrl |= ARM1176_AUXCTL_FIO;

		/* 714068 */
		auxctrl |= ARM1176_AUXCTL_PHD;
	}

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm volatile ("mcr\tp15, 0, %0, c7, c7, 0" : : "r"(sbz));

	/* Allow detection code to find the VFP if it's fitted.  */
	armreg_cpacr_write(0x0fffffff);

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(~cpuctrl_wax, cpuctrl);

	/* Update auxctlr */
	armreg_auxctl_write(auxctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARM1136 || CPU_ARM1176 */

#ifdef CPU_SA110
struct cpu_option sa110_options[] = {
#ifdef COMPAT_12
	{ "nocache",		IGN, BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "nowritebuf",		IGN, BIC, CPU_CONTROL_WBUF_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sa110.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sa110.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "sa110.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "sa110.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
sa110_setup(char *args)
{
	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, sa110_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
#if 0
	cpu_control(cpuctrlmask, cpuctrl);
#endif
	cpu_control(0xffffffff, cpuctrl);

	/*
	 * enable clockswitching, note that this doesn't read or write to r0,
	 * r0 is just to make it valid asm
	 */
	__asm volatile ("mcr p15, 0, r0, c15, c1, 2");
}
#endif	/* CPU_SA110 */

#if defined(CPU_SA1100) || defined(CPU_SA1110)
struct cpu_option sa11x0_options[] = {
#ifdef COMPAT_12
	{ "nocache",		IGN, BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "nowritebuf",		IGN, BIC, CPU_CONTROL_WBUF_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sa11x0.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sa11x0.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "sa11x0.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "sa11x0.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
sa11x0_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_VECRELOC;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, sa11x0_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_SA1100 || CPU_SA1110 */

#if defined(CPU_FA526)
struct cpu_option fa526_options[] = {
#ifdef COMPAT_12
	{ "nocache",		IGN, BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "nowritebuf",		IGN, BIC, CPU_CONTROL_WBUF_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
fa526_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_VECRELOC;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, fa526_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_FA526 */

#if defined(CPU_IXP12X0)
struct cpu_option ixp12x0_options[] = {
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "ixp12x0.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "ixp12x0.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "ixp12x0.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "ixp12x0.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
ixp12x0_setup(char *args)
{

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE;

	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_DC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_IC_ENABLE
		 | CPU_CONTROL_VECRELOC;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, ixp12x0_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	/* cpu_control(0xffffffff, cpuctrl); */
	cpu_control(cpuctrlmask, cpuctrl);
}
#endif /* CPU_IXP12X0 */

#if defined(CPU_XSCALE)
struct cpu_option xscale_options[] = {
#ifdef COMPAT_12
	{ "branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "nocache",		IGN, BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
#endif	/* COMPAT_12 */
	{ "cpu.branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "xscale.branchpredict", BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "xscale.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "xscale.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "xscale.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
xscale_setup(char *args)
{
	uint32_t auxctl;

	/*
	 * The XScale Write Buffer is always enabled.  Our option
	 * is to enable/disable coalescing.  Note that bits 6:3
	 * must always be enabled.
	 */

	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE
		 | CPU_CONTROL_BPRD_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_VECRELOC;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, xscale_options, cpuctrl);

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/*
	 * Set the control register.  Note that bits 6:3 must always
	 * be set to 1.
	 */
	curcpu()->ci_ctrl = cpuctrl;
#if 0
	cpu_control(cpuctrlmask, cpuctrl);
#endif
	cpu_control(0xffffffff, cpuctrl);

	/* Make sure write coalescing is turned on */
	auxctl = armreg_auxctl_read();
#ifdef XSCALE_NO_COALESCE_WRITES
	auxctl |= XSCALE_AUXCTL_K;
#else
	auxctl &= ~XSCALE_AUXCTL_K;
#endif
	armreg_auxctl_write(auxctl);
}
#endif	/* CPU_XSCALE */

#if defined(CPU_SHEEVA)
struct cpu_option sheeva_options[] = {
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sheeva.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sheeva.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "sheeva.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "sheeva.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
sheeva_setup(char *args)
{
	int cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_BPRD_ENABLE;
#if 0
	int cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
	    | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_ROUNDROBIN | CPU_CONTROL_CPCLK;
#endif

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	cpuctrl = parse_cpu_options(args, sheeva_options, cpuctrl);

	/* Enable DCache Streaming Switch and Write Allocate */
	uint32_t sheeva_ext = armreg_sheeva_xctrl_read();

	sheeva_ext |= FC_DCACHE_STREAM_EN | FC_WR_ALLOC_EN;
#ifdef SHEEVA_L2_CACHE
	sheeva_ext |= FC_L2CACHE_EN;
	sheeva_ext &= ~FC_L2_PREF_DIS;
#endif

	armreg_sheeva_xctrl_write(sheeva_ext);

#ifdef SHEEVA_L2_CACHE
#ifndef SHEEVA_L2_CACHE_WT
	arm_scache.cache_type = CPU_CT_CTYPE_WB2;
#elif CPU_CT_CTYPE_WT != 0
	arm_scache.cache_type = CPU_CT_CTYPE_WT;
#endif
	arm_scache.cache_unified = 1;
	arm_scache.dcache_type = arm_scache.icache_type = CACHE_TYPE_PIPT;
	arm_scache.dcache_size = arm_scache.icache_size = 256*1024;
	arm_scache.dcache_ways = arm_scache.icache_ways = 4;
	arm_scache.dcache_way_size = arm_scache.icache_way_size =
	    arm_scache.dcache_size / arm_scache.dcache_ways;
	arm_scache.dcache_line_size = arm_scache.icache_line_size = 32;
	arm_scache.dcache_sets = arm_scache.icache_sets =
	    arm_scache.dcache_way_size / arm_scache.dcache_line_size;

	cpufuncs.cf_sdcache_wb_range = sheeva_sdcache_wb_range;
	cpufuncs.cf_sdcache_inv_range = sheeva_sdcache_inv_range;
	cpufuncs.cf_sdcache_wbinv_range = sheeva_sdcache_wbinv_range;
#endif /* SHEEVA_L2_CACHE */

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

#ifndef ARM_HAS_VBAR
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
#ifdef SHEEVA_L2_CACHE
	sheeva_sdcache_wbinv_all();
#endif
}
#endif	/* CPU_SHEEVA */
