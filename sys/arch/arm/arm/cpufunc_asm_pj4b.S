/*	$NetBSD: cpufunc_asm_pj4b.S,v 1.11 2015/05/20 02:59:57 hsuenaga Exp $ */

/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

Developed by Semihalf

********************************************************************************
Marvell BSD License

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
            this list of conditions and the following disclaimer.

    *   Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    *   Neither the name of Marvell nor the names of its contributors may be
        used to endorse or promote products derived from this software without
        specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "assym.h"
#include <arm/asm.h>
#include <arm/locore.h>

#define MV_FMC0_SMP		(1 << 1) /* SMP/nAMP enable */
#define MV_FMC0_PARITY		(1 << 2) /* Enable L1 Cache Parity */
#define MV_FMC0_LFDIS		(1 << 7) /* Disable DC Speculative linefill*/
#define MV_FMC0_FW		(1 << 8) /* Cache & TLB maintenance broadcast */

#define MPIDR_CPUID_MASK	(0x3 << 0) /* CPUID */
#define MPIDR_CLUSTERID_MASK	(0xf << 8) /* CLUSTERID */
#define MPIDR_UNI_PROCESSOR	(1 << 30)  /* Uni-Processor System */
#define MPIDR_MPCORE		(1 << 31)  /* New Style MPCore like CortexA9 */

/* LINTSTUB: void pj4b_cpu_sleep(int); */
ENTRY(pj4b_cpu_sleep)
	dsb
	wfi				@ wait for an interrupt
	dsb				@ Erratum#ARM-CPU-4742
	b	irq_idle_entry		@ assume we got an interrupt
END(pj4b_cpu_sleep)

/* LINTSTUB: void pj4b_config(void); */
ENTRY(pj4b_config)
	/* Set Marvell Auxiliary Debug Modes Control 0 register */
	mrc	p15, 1, r0, c15, c1, 0
	bic	r0, r0, #(1 << 12)	@ Erratum#ARM-CPU-6136
					@ LDSTM 1st issue is single word
	orr	r0, r0, #(1 << 22)	@ DVM_WAKEUP enable
	mcr	p15, 1, r0, c15, c1, 0

	/* Set Marvell Auxiliary Debug Modes Control 1 register */
	mrc	p15, 1, r0, c15, c1, 1
	bic	r0, r0, #(1 << 2)	@ Erratum#ARM-CPU-6409
					@ Disable static branch prediction
	orr	r0, r0, #(1 << 5)	@ STREX backoff disable
	mcr	p15, 1, r0, c15, c1, 1

	/* Set Marvell Auxiliary Debug Modes Control 2 register */
	mrc	p15, 1, r0, c15, c1, 2
	bic	r0, r0, #(1 << 23)	@ Enable fast LDR
	orr	r0, r0, #(1 << 25)	@ Intervention Interleave disable
	orr	r0, r0, #(1 << 27)	@ Critical word 1st sequencing dis.
	orr	r0, r0, #(1 << 29)	@ Disable MO device R/W
	orr	r0, r0, #(1 << 30)	@ L1 cache strict round-robin 
	orr	r0, r0, #(1 << 31)	@ Enable write evict
	mcr	p15, 1, r0, c15, c1, 2

	/* Set Marvell Auxiliary Function Modes Control 0 register */
	mrc	p15, 1, r0, c15, c2, 0		@ get FMC0
	mrc	p15, 0, ip, c0, c0, 5		@ get MPIDR
	tst	ip, #PJ4B_MPIDR_MP
	beq	1f				@ if not set, not a MPCORE
	tst	ip, #PJ4B_MPIDR_U
	bne	1f				@ if set, uni-processor system 
	orr	r0, r0, #(PJ4B_AUXFMC0_SMPNAMP)	@ enable SMP/nAMP
	orr	r0, r0, #(PJ4B_AUXFMC0_FW)	@ enable maintenance bcast
1:
	bic	r0, r0, #(PJ4B_AUXFMC0_DCSLFD)	@ enable speculative linefill
	orr	r0, r0, #(PJ4B_AUXFMC0_L1PARITY)	@ enable L1 parity
	mcr	p15, 1, r0, c15, c2, 0

	RET
END(pj4b_config)

/* LINTSTUB: void pj4b_io_coherency_barrier(vaddr_t, paddr_t, vsize_t); */
ENTRY_NP(pj4b_io_coherency_barrier)
	movw	r0, #:lower16:_C_LABEL(armadaxp_l2_barrier_reg)
	movt	r0, #:upper16:_C_LABEL(armadaxp_l2_barrier_reg)
	ldr	r0, [r0]	@ MVSOC_MLMB_CIB_BARRIER
	mov	r1, #1		@ MVSOC_MLMB_CIB_BARRIER_TRIGGER
	str	r1, [r0]
1:
	ldr	r1, [r0]
	tst	r1, #1
	bne	1b		@ if set, CIB is busy.
	dsb
	RET
END(pj4b_io_coherency_barrier)

STRONG_ALIAS(pj4b_dcache_cfu_wbinv_range, pj4b_io_coherency_barrier)
STRONG_ALIAS(pj4b_dcache_cfu_inv_range, pj4b_io_coherency_barrier)
STRONG_ALIAS(pj4b_dcache_cfu_wb_range, pj4b_io_coherency_barrier)
