/*	$NetBSD: cpufunc_asm_arm11.S,v 1.17 2014/10/29 16:22:31 skrll Exp $	*/

/*
 * Copyright (c) 2002, 2005 ARM Limited
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
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * ARM11 assembly functions for CPU / MMU / TLB specific operations
 */

#include "assym.h"
#include <machine/asm.h>
#include <arm/locore.h>

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(arm11_setttb)
#ifdef PMAP_CACHE_VIVT
#error arm11 does not have a VIVT cache.
#endif

	cmp	r1, #0
	mcr	p15, 0, r0, c2, c0, 0	/* set the new TTBR0 */
#ifdef ARM_MMU_EXTENDED
	mcreq	p15, 0, r0, c2, c0, 1	/* set the new TTBR1 */
#else
	mcrne	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	mcrne	p15, 0, r0, c7, c10, 4	/* drain write buffer */
#endif
	RET
END(arm11_setttb)

/*
 * Context switch.
 *
 * These are the CPU-specific parts of the context switcher cpu_switchto()
 * These functions actually perform the TTB reload.
 */
ENTRY(arm11_context_switch)
	/*
	 * We can assume that the caches will only contain kernel addresses
	 * at this point.  So no need to flush them again.
	 */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mcr	p15, 0, r0, c2, c0, 0	/* set the new TTBR0 */
#ifdef ARM_MMU_EXTENDED
	cmp	r1, #0
	mcreq	p15, 0, r0, c2, c0, 1	/* set the new TTBR1 */
#else
	mcr	p15, 0, r0, c8, c7, 0	/* and flush the I+D tlbs */
#endif

	/* Paranoia -- make sure the pipeline is empty. */
	nop
	nop
	nop
	RET
END(arm11_context_switch)

/*
 * TLB functions
 */

ENTRY(arm11_tlb_flushI)
	mov	r0, #0
	mcr	p15, 0, r0, c8, c5, 0	/* flush I tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_tlb_flushI)

ENTRY(arm11_tlb_flushI_SE)
#ifdef ARM_MMU_EXTENDED
	bic	r0, r0, #0xff
	bic	r0, r0, #0xf00		/* Always KERNEL_PID, i.e. 0 */
#endif
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
#if PAGE_SIZE == 2 * L2_S_SIZE
	add	r0, r0, #L2_S_SIZE
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
#endif

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_tlb_flushI_SE)

ENTRY(arm11_tlb_flushD)
	mov	r0, #0
	mcr	p15, 0, r0, c8, c6, 0	/* flush D tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_tlb_flushD)

ENTRY(arm11_tlb_flushD_SE)
#ifdef ARM_MMU_EXTENDED
	bic	r0, r0, #0xff
	bic	r0, r0, #0xf00		/* Always KERNEL_PID, i.e. 0 */
#endif
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
#if PAGE_SIZE == 2 * L2_S_SIZE
	add	r0, r0, #L2_S_SIZE
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
#endif
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_tlb_flushD_SE)

ENTRY(arm11_tlb_flushID)
	mov	r0, #0
	mcr	p15, 0, r0, c8, c7, 0	/* flush I+D tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_tlb_flushID)

ENTRY(arm11_tlb_flushID_SE)
#ifdef ARM_MMU_EXTENDED
	bic	r0, r0, #0xff
	bic	r0, r0, #0xf00		/* Always KERNEL_PID, i.e. 0 */
#endif
	mcr	p15, 0, r0, c8, c7, 1	/* flush I+D tlb single entry */
#if PAGE_SIZE == 2 * L2_S_SIZE
	add	r0, r0, #L2_S_SIZE
	mcr	p15, 0, r0, c8, c7, 1	/* flush I+D tlb single entry */
#endif
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_tlb_flushID_SE)

#ifdef ARM_MMU_EXTENDED
ENTRY(arm11_tlb_flushID_ASID)
	mcr	p15, 0, r0, c8, c7, 2	/* flush I+D tlb */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_tlb_flushID_ASID)
#endif

/*
 * Other functions
 */
ENTRY(arm11_drain_writebuf)
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	RET
END(arm11_drain_writebuf)

ENTRY_NP(arm11_sleep)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c0, 4	/* wait for interrupt */
	RET
END(arm11_sleep)
