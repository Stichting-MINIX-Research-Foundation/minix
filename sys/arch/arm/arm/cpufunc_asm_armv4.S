/*	$NetBSD: cpufunc_asm_armv4.S,v 1.5 2014/03/30 01:15:03 matt Exp $	*/

/*
 * Copyright (c) 2001 ARM Limited
 * Copyright (c) 1997,1998 Mark Brinicombe.
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
 * ARMv4 assembly functions for CPU / MMU / TLB specific operations
 */
 
#include "assym.h"
#include <machine/asm.h>
#include <arm/locore.h>

/*
 * TLB functions
 */
ENTRY(armv4_tlb_flushID)
	mcr	p15, 0, r0, c8, c7, 0	/* flush I+D tlb */
	mov	pc, lr
END(armv4_tlb_flushID)

ENTRY(armv4_tlb_flushI)
	mov	r0, #0
	mcr	p15, 0, r0, c8, c5, 0	/* flush I tlb */
	mov	pc, lr
END(armv4_tlb_flushI)

ENTRY(armv4_tlb_flushD)
	mov	r0, #0
	mcr	p15, 0, r0, c8, c6, 0	/* flush D tlb */
	mov	pc, lr
END(armv4_tlb_flushD)

ENTRY(armv4_tlb_flushD_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
#if PAGE_SIZE == 2 * L2_S_SIZE
	add	r0, r0, #L2_S_SIZE
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
#endif
	mov	pc, lr
END(armv4_tlb_flushD_SE)

/*
 * Other functions
 */
ENTRY(armv4_drain_writebuf)
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr
END(armv4_drain_writebuf)
