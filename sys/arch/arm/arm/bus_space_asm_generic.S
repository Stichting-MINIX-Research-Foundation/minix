/*	$NetBSD: bus_space_asm_generic.S,v 1.11 2013/10/28 22:50:25 matt Exp $	*/

/*
 * Copyright (c) 1997 Causality Limited.
 * Copyright (c) 1997 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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
 */

#include <arm/asm.h>
#include <arm/cpuconf.h>
#include <arm/byte_swap.h>

#ifdef _ARM_ARCH_7
#define	DSB	dsb
#elif defined(_ARM_ARCH_6)
#define	DSB	mcr p15, 0, r0, c7, c10, 4
#else
#define	DSB
#endif

#if defined(__thumb__) && defined(_ARM_ARCH_T2)
#define CHECK_LENGTH(r, l)	cbz r, l
#elif defined(__thumb__)
#define CHECK_LENGTH(r, l)	cmp r, #0; beq l
#else
#define CHECK_LENGTH(r, l)	cmp r, #0; RETc(eq)
#endif
/*
 * Generic bus_space functions.
 */

/*
 * read single
 */

ENTRY_NP(generic_bs_r_1)
	DSB
	ldrb	r0, [r1, r2]
	RET
END(generic_bs_r_1)

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_r_2)
	DSB
	ldrh	r0, [r1, r2]
	RET
END(generic_armv4_bs_r_2)

ENTRY_NP(generic_armv4_bs_r_2_swap)
	DSB
	ldrh	r0, [r1, r2]
	BSWAP16(r0, r0, r1)
	RET
END(generic_armv4_bs_r_2_swap)
#endif

ENTRY_NP(generic_bs_r_4)
	DSB
	ldr	r0, [r1, r2]
	RET
END(generic_bs_r_4)

ENTRY_NP(generic_bs_r_4_swap)
	DSB
	ldr	r0, [r1, r2]
	BSWAP32(r0, r0, r1)
	RET
END(generic_bs_r_4_swap)

/*
 * write single
 */

ENTRY_NP(generic_bs_w_1)
	strb	r3, [r1, r2]
	DSB
	RET
END(generic_bs_w_1)

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_w_2_swap)
	BSWAP16(r3, r3, r0)		/* swap and fallthrough */
ENTRY_NP(generic_armv4_bs_w_2)
	strh	r3, [r1, r2]
	DSB
	RET
END(generic_armv4_bs_w_2)
END(generic_armv4_bs_w_2_swap)
#endif

ENTRY_NP(generic_bs_w_4_swap)
	BSWAP32(r3, r3, r0)
ENTRY_NP(generic_bs_w_4)
	str	r3, [r1, r2]
	DSB
	RET
END(generic_bs_w_4)
END(generic_bs_w_4_swap)

/*
 * read multiple
 */

ENTRY_NP(generic_bs_rm_1)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldrb	r3, [r0]
	strb	r3, [r1], #1
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_bs_rm_1)

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_rm_2)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldrh	r3, [r0]
	strh	r3, [r1], #2
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_armv4_bs_rm_2)

ENTRY_NP(generic_armv4_bs_rm_2_swap)
	DSB
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldrh	r3, [r0]
	BSWAP16(r3, r3, ip)
	strh	r3, [r1], #2
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_armv4_bs_rm_2_swap)
#endif

ENTRY_NP(generic_bs_rm_4)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldr	r3, [r0]
	str	r3, [r1], #4
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_bs_rm_4)

ENTRY_NP(generic_bs_rm_4_swap)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldr	r3, [r0]
	BSWAP32(r3, r3, ip)
	str	r3, [r1], #4
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_bs_rm_4_swap)

/*
 * write multiple
 */

ENTRY_NP(generic_bs_wm_1)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldrb	r3, [r1], #1
	strb	r3, [r0]
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_wm_1)

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_wm_2)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldrh	r3, [r1], #2
	strh	r3, [r0]
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_armv4_bs_wm_2)

ENTRY_NP(generic_armv4_bs_wm_2_swap)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]

1:	ldrh	r3, [r1], #2
	BSWAP16(r3, r3, ip)
	strh	r3, [r0]
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_armv4_bs_wm_2_swap)
#endif

ENTRY_NP(generic_bs_wm_4)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldr	r3, [r1], #4
	str	r3, [r0]
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_wm_4)

ENTRY_NP(generic_bs_wm_4_swap)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldr	r3, [r1], #4
	BSWAP32(r3, r3, ip)
	str	r3, [r0]
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_wm_4_swap)

/*
 * read region
 */

ENTRY_NP(generic_bs_rr_1)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldrb	r3, [r0], #1
	strb	r3, [r1], #1
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_bs_rr_1)

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_rr_2)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldrh	r3, [r0], #2
	strh	r3, [r1], #2
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_armv4_bs_rr_2)

ENTRY_NP(generic_armv4_bs_rr_2_swap)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldrh	r3, [r0], #2
	BSWAP16(r3, r3, ip)
	strh	r3, [r1], #2
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_armv4_bs_rr_2_swap)
#endif

ENTRY_NP(generic_bs_rr_4)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldr	r3, [r0], #4
	str	r3, [r1], #4
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_bs_rr_4)

ENTRY_NP(generic_bs_rr_4_swap)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)
	DSB

1:	ldr	r3, [r0], #4
	BSWAP32(r3, r3, ip)
	str	r3, [r1], #4
	subs	r2, r2, #1
	bne	1b

99:	RET
END(generic_bs_rr_4_swap)

/*
 * write region.
 */

ENTRY_NP(generic_bs_wr_1)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_wr_1)

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_wr_2)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldrh	r3, [r1], #2
	strh	r3, [r0], #2
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_armv4_bs_wr_2)

ENTRY_NP(generic_armv4_bs_wr_2_swap)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldrh	r3, [r1], #2
	BSWAP16(r3, r3, ip)
	strh	r3, [r0], #2
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_armv4_bs_wr_2_swap)
#endif

ENTRY_NP(generic_bs_wr_4)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldr	r3, [r1], #4
	str	r3, [r0], #4
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_wr_4)

ENTRY_NP(generic_bs_wr_4_swap)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	ldr	r3, [r1], #4
	BSWAP32(r3, r3, ip)
	str	r3, [r0], #4
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_wr_4_swap)

/*
 * set region
 */

ENTRY_NP(generic_bs_sr_1)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	strb	r1, [r0], #1
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_sr_1)

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_sr_2_swap)
	BSWAP16(r3, r3, r0)	/* swap and fallthrough */
ENTRY_NP(generic_armv4_bs_sr_2)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	strh	r1, [r0], #2
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_armv4_bs_sr_2)
END(generic_armv4_bs_sr_2_swap)
#endif

ENTRY_NP(generic_bs_sr_4_swap)
	BSWAP32(r3, r3, r0)	/* swap and fallthrough */
ENTRY_NP(generic_bs_sr_4)
	adds	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	CHECK_LENGTH(r2, 99f)

1:	str	r1, [r0], #4
	subs	r2, r2, #1
	bne	1b

	DSB
99:	RET
END(generic_bs_sr_4)
END(generic_bs_sr_4_swap)

/*
 * copy region
 */

#if (ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 + ARM_ARCH_7) > 0
ENTRY_NP(generic_armv4_bs_c_2)
	adds	r0, r1, r2
	ldr	r2, [sp, #0]
	adds	r1, r2, r3
	ldr	r2, [sp, #4]
	CHECK_LENGTH(r2, 99f)

	lsls	r2, r2, #1

	cmp	r0, r1
	blt	2f

	adds	r0, r0, r2
	adds	r1, r1, r2
	negs	r2, r2	

1:	ldrh	r3, [r0, r2]
	strh	r3, [r1, r2]
	adds	r2, r2, #2
	bne	1b

	DSB
	RET

2:	subs	r2, r2, #2
	ldrh	r3, [r0, r2]
	strh	r3, [r1, r2]
	bne	2b

	DSB
99:	RET
END(generic_armv4_bs_c_2)
#endif
