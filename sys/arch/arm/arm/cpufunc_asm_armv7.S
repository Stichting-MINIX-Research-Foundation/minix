/*-
 * Copyright (c) 2010 Per Odlund <per.odlund@armagedon.se>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* ARMv7 assembly functions for manipulating caches and other core functions.
 * Based on cpufuncs for v6 and xscale.
 */

#include "assym.h"
#include <machine/asm.h>
#include <arm/locore.h>

	.arch	armv7a

ENTRY(armv7_cpu_sleep)
	dsb
	wfi				@ wait for an interrupt
	b	irq_idle_entry		@ assume we got an interrupt
END(armv7_cpu_sleep)

ENTRY(armv7_wait)
	mrc	p15, 0, r0, c2, c0, 0	@ arbitrary read of CP15
	add	r0, r0, #0		@ a stall
	bx	lr
END(armv7_wait)

ENTRY(armv7_context_switch)
	dsb				@ data synchronization barrier
	mrc	p15, 0, ip, c0, c0, 5	@ get MPIDR
	cmp	ip, #0
	orrlt	r0, r0, #TTBR_MPATTR	@ MP, cachable (Normal WB)
	orrge	r0, r0, #TTBR_UPATTR	@ Non-MP, cacheable, normal WB
	mcr	p15, 0, r0, c2, c0, 0 	@ set the new TTBR 0
#ifdef ARM_MMU_EXTENDED
	cmp	r1, #0
	mcreq	p15, 0, r0, c2, c0, 1   @ set the new TTBR 1
#else
	mcr	p15, 0, r0, c8, c7, 0	@ flush the I+D
#endif
	dsb
	isb
	bx	lr
END(armv7_context_switch)

#ifdef ARM_MMU_EXTENDED_XXX
ENTRY(armv7_tlb_flushID_ASID)
#ifdef MULTIPROCESSOR
	mcr	p15, 0, r0, c8, c3, 2	@ flush I+D tlb all ASID
#else
	mcr	p15, 0, r0, c8, c7, 2	@ flush I+D tlb all ASID
#endif
	dsb				@ data synchronization barrier
	isb
	bx	lr
END(armv7_tlb_flushID_ASID)
#endif

STRONG_ALIAS(armv7_tlb_flushD_SE, armv7_tlb_flushID_SE)
STRONG_ALIAS(armv7_tlb_flushI_SE, armv7_tlb_flushID_SE)
ENTRY(armv7_tlb_flushID_SE)
	bfc	r0, #0, #12		@ Always KERNEL_PID, i.e. 0
#ifdef MULTIPROCESSOR
	mcr	p15, 0, r0, c8, c3, 1	@ flush I+D tlb single entry
#if PAGE_SIZE == 2*L2_S_SIZE
	add	r0, r0, #L2_S_SIZE
	mcr	p15, 0, r0, c8, c3, 1	@ flush I+D tlb single entry
#endif
#else /* !MULTIPROCESSOR */
	mcr	p15, 0, r0, c8, c7, 1	@ flush I+D tlb single entry
#if PAGE_SIZE == 2*L2_S_SIZE
	add	r0, r0, #L2_S_SIZE
	mcr	p15, 0, r0, c8, c7, 1	@ flush I+D tlb single entry
#endif
#endif /* !MULTIPROCESSOR */
	dsb				@ data synchronization barrier
	isb
	bx	lr
END(armv7_tlb_flushID_SE)

ENTRY(armv7_tlb_flushD)
	mov	r0, #0
	mcr	p15, 0, r0, c8, c6, 0	@ flush entire D tlb
	dsb				@ data synchronization barrier
	isb
	bx      lr
END(armv7_tlb_flushD)

STRONG_ALIAS(armv7_tlb_flushI, armv7_tlb_flushID)
ENTRY(armv7_tlb_flushID)
	dsb
	mov	r0, #0
#ifdef MULTIPROCESSOR
	mcr	p15, 0, r0, c8, c3, 0	@ flush entire I+D tlb, IS
	mcr	p15, 0, r0, c7, c1, 6	@ branch predictor invalidate, IS
#else
	mcr	p15, 0, r0, c8, c7, 0	@ flush entire I+D tlb
	mcr	p15, 0, r0, c7, c5, 6	@ branch predictor invalidate
#endif
	dsb				@ data synchronization barrier
	isb
	bx      lr
END(armv7_tlb_flushID)


ENTRY_NP(armv7_setttb)
	mrc	p15, 0, ip, c0, c0, 5	@ get MPIDR
	cmp	ip, #0
	orrlt	r0, r0, #TTBR_MPATTR	@ MP, cachable (Normal WB)
	orrge	r0, r0, #TTBR_UPATTR	@ Non-MP, cacheable, normal WB
	mcr	p15, 0, r0, c2, c0, 0   @ load new TTBR 0
#ifdef ARM_MMU_EXTENDED
	cmp	r1, #0
	mcreq	p15, 0, r0, c2, c0, 1   @ load new TTBR 1
#else
	mcr	p15, 0, r0, c8, c7, 0   @ invalidate all I+D TLBs
#endif
	dsb				@ data synchronization barrier
	isb
	bx	lr
END(armv7_setttb)

/* Other functions. */

ENTRY_NP(armv7_drain_writebuf)
	dsb				@ data synchronization barrier
	RET
END(armv7_drain_writebuf)

/* Cache operations. */

/* LINTSTUB: void armv7_icache_sync_range(vaddr_t, vsize_t); */
ENTRY_NP(armv7_icache_sync_range)
	mov	ip, #CPU_CSSR_InD
	mcr	p15, 2, ip, c0, c0, 0	@ set cache level to L1-I
	mrc	p15, 1, r2, c0, c0, 0	@ read CCSIDR
	mov	ip, #0
	mcr	p15, 2, ip, c0, c0, 0	@ set cache level to L1-D
	mrc	p15, 1, r3, c0, c0, 0	@ read CCSIDR
	and	r2, r2, #7		@ get line size (log2(size)-4, 0=16)
	and	r3, r3, #7		@ get line size (log2(size)-4, 0=16)
	cmp	r2, r3			@ compare ilinesize to dlinesize
	movgt	r2, r3			@ pick lesser of the two
	mov	ip, #16			@ make a bit mask
	lsl	r2, ip, r2		@ and shift into position
	sub	ip, r2, #1		@ make into a mask
	and	r3, r0, ip		@ get offset into cache line
	add	r1, r1, r3		@ add to length
	bic	r0, r0, ip		@ clear offset from start.
1:
	mcr	p15, 0, r0, c7, c10, 1	@ wb the D-Cache line
	mcr	p15, 0, r0, c7, c5, 1	@ invalidate the I-Cache line
	add	r0, r0, r2
	subs	r1, r1, r2
	bhi	1b

	dsb				@ data synchronization barrier
	isb
	bx	lr
END(armv7_icache_sync_range)

/* LINTSTUB: void armv7_icache_sync_all(void); */
ENTRY_NP(armv7_icache_sync_all)
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache cleaning code.
	 */
	stmdb	sp!, {r0, lr}
	bl	_C_LABEL(armv7_idcache_wbinv_all) @clean the D cache
	ldmia	sp!, {r0, lr}
	dsb				@ data synchronization barrier
	isb
	bx	lr
END(armv7_icache_sync_all)

ENTRY(armv7_dcache_wb_range)
	mov	ip, #0
	mcr	p15, 2, ip, c0, c0, 0	@ set cache level to L1
	mrc	p15, 1, r2, c0, c0, 0	@ read CCSIDR
	and	r2, r2, #7		@ get line size (log2(size)-4, 0=16)
	mov	ip, #16			@ make a bit mask
	lsl	r2, ip, r2		@ and shift into position
	sub	ip, r2, #1		@ make into a mask
	and	r3, r0, ip		@ get offset into cache line
	add	r1, r1, r3		@ add to length
	bic	r0, r0, ip		@ clear offset from start.
	dsb
1:
	mcr	p15, 0, r0, c7, c10, 1	@ wb the D-Cache to PoC
	add	r0, r0, r2
	subs	r1, r1, r2
	bhi	1b
	dsb				@ data synchronization barrier
	bx	lr
END(armv7_dcache_wb_range)

/* LINTSTUB: void armv7_dcache_wbinv_range(vaddr_t, vsize_t); */
ENTRY(armv7_dcache_wbinv_range)
	mov	ip, #0
	mcr	p15, 2, ip, c0, c0, 0	@ set cache level to L1
	mrc	p15, 1, r2, c0, c0, 0	@ read CCSIDR
	and	r2, r2, #7		@ get line size (log2(size)-4, 0=16)
	mov	ip, #16			@ make a bit mask
	lsl	r2, ip, r2		@ and shift into position
	sub	ip, r2, #1		@ make into a mask
	and	r3, r0, ip		@ get offset into cache line
	add	r1, r1, r3		@ add to length
	bic	r0, r0, ip		@ clear offset from start.
	dsb
1:
	mcr	p15, 0, r0, c7, c14, 1	@ wb and inv the D-Cache line to PoC
	add	r0, r0, r2
	subs	r1, r1, r2
	bhi	1b
	dsb				@ data synchronization barrier
	bx	lr
END(armv7_dcache_wbinv_range)

/* * LINTSTUB: void armv7_dcache_inv_range(vaddr_t, vsize_t); */
ENTRY(armv7_dcache_inv_range)
	mov	ip, #0
	mcr	p15, 2, ip, c0, c0, 0	@ set cache level to L1
	mrc	p15, 1, r2, c0, c0, 0	@ read CCSIDR
	and	r2, r2, #7		@ get line size (log2(size)-4, 0=16)
	mov	ip, #16			@ make a bit mask
	lsl	r2, ip, r2		@ and shift into position
	sub	ip, r2, #1		@ make into a mask
	and	r3, r0, ip		@ get offset into cache line
	add	r1, r1, r3		@ add to length
	bic	r0, r0, ip		@ clear offset from start.
1:
	mcr	p15, 0, r0, c7, c6, 1	@ invalidate the D-Cache line
	add	r0, r0, r2
	subs	r1, r1, r2
	bhi	1b

	dsb				@ data synchronization barrier
	bx	lr
END(armv7_dcache_inv_range)


/* * LINTSTUB: void armv7_idcache_wbinv_range(vaddr_t, vsize_t); */
ENTRY(armv7_idcache_wbinv_range)
	mov	ip, #0
	mcr	p15, 2, ip, c0, c0, 0	@ set cache level to L1
	mrc	p15, 1, r2, c0, c0, 0	@ read CCSIDR
	and	r2, r2, #7		@ get line size (log2(size)-4, 0=16)
	mov	ip, #16			@ make a bit mask
	lsl	r2, ip, r2		@ and shift into position
	sub	ip, r2, #1		@ make into a mask
	and	r3, r0, ip		@ get offset into cache line
	add	r1, r1, r3		@ add to length
	bic	r0, r0, ip		@ clear offset from start.
	dsb
1:
	mcr	p15, 0, r0, c7, c5, 1	@ invalidate the I-Cache line
	mcr	p15, 0, r0, c7, c14, 1 	@ wb and inv the D-Cache line
	add	r0, r0, r2
	subs	r1, r1, r2
	bhi	1b

	dsb				@ data synchronization barrier
	isb
	bx	lr
END(armv7_idcache_wbinv_range)

/* * LINTSTUB: void armv7_idcache_wbinv_all(void); */
ENTRY_NP(armv7_idcache_wbinv_all)
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache purging code.
	 */
	dmb
	mcr	p15, 0, r0, c7, c5, 0
	b	_C_LABEL(armv7_dcache_wbinv_all)
END(armv7_idcache_wbinv_all)

/*
 * These work very hard to not push registers onto the stack
 * and to limit themselves to use r0-r3 and ip.
 */
/* * LINTSTUB: void armv7_icache_inv_all(void); */
ENTRY_NP(armv7_icache_inv_all)
	mov	r0, #0
	mcr	p15, 2, r0, c0, c0, 0	@ set cache level to L1
	mrc	p15, 1, r0, c0, c0, 0	@ read CCSIDR

	ubfx	r2, r0, #13, #15	@ get num sets - 1 from CCSIDR
	ubfx	r3, r0, #3, #10		@ get numways - 1 from CCSIDR
	clz	r1, r3			@ number of bits to MSB of way
	lsl	r3, r3, r1		@ shift into position
	mov	ip, #1			@
	lsl	ip, ip, r1		@ ip now contains the way decr

	ubfx	r0, r0, #0, #3		@ get linesize from CCSIDR
	add	r0, r0, #4		@ apply bias
	lsl	r2, r2, r0		@ shift sets by log2(linesize)
	add	r3, r3, r2		@ merge numsets - 1 with numways - 1
	sub	ip, ip, r2		@ subtract numsets - 1 from way decr
	mov	r1, #1
	lsl	r1, r1, r0		@ r1 now contains the set decr
	mov	r2, ip			@ r2 now contains set way decr

	/* r3 = ways/sets, r2 = way decr, r1 = set decr, r0 and ip are free */
1:	mcr	p15, 0, r3, c7, c6, 2	@ invalidate line
	movs	r0, r3			@ get current way/set
	beq	2f			@ at 0 means we are done.
	lsls	r0, r0, #10		@ clear way bits leaving only set bits
	subne	r3, r3, r1		@ non-zero?, decrement set #
	subeq	r3, r3, r2		@ zero?, decrement way # and restore set count
	b	1b

2:	dsb				@ wait for stores to finish
	mov	r0, #0			@ and ...
	mcr	p15, 0, r0, c7, c5, 0	@ invalidate L1 cache
	isb				@ instruction sync barrier
	bx	lr			@ return
END(armv7_icache_inv_all)

/* * LINTSTUB: void armv7_dcache_l1inv_all(void); */
ENTRY_NP(armv7_dcache_l1inv_all)
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	and	r0, r0, #0x7		@ check L1
	bxeq	lr			@ return if no L1 cache
	mov	r3, #0			@ start with L1
	mcr	p15, 2, r3, c0, c0, 0	@ select cache level
	isb
	mrc	p15, 1, r0, c0, c0, 0	@ read CCSIDR

	ubfx	ip, r0, #0, #3		@ get linesize from CCSIDR
	add	ip, ip, #4		@ apply bias
	ubfx	r2, r0, #13, #15	@ get numsets - 1 from CCSIDR
	lsl	r2, r2, ip		@ shift to set position
	orr	r3, r3, r2		@ merge set into way/set/level
	mov	r1, #1
	lsl	r1, r1, ip		@ r1 = set decr

	ubfx	ip, r0, #3, #10		@ get numways - 1 from [to be discarded] CCSIDR
	clz	r2, ip			@ number of bits to MSB of way
	lsl	ip, ip, r2		@ shift by that into way position
	mov	r0, #1			@
	lsl	r2, r0, r2		@ r2 now contains the way decr
	mov	r0, r3 			@ get sets/level (no way yet)
	orr	r3, r3, ip		@ merge way into way/set/level
	bfc	r0, #0, #4		@ clear low 4 bits (level) to get numset - 1
	sub	r2, r2, r0		@ subtract from way decr

	/* r3 = ways/sets/level, r2 = way decr, r1 = set decr, r0 and ip are free */
1:	mcr	p15, 0, r3, c7, c6, 2	@ invalidate line
	cmp	r3, #15			@ are we done with this level (way/set == 0)
	bls	.Ldone_l1inv		@ yes, we've finished
	ubfx	r0, r3, #4, #18		@ extract set bits
	cmp	r0, #0			@ compare
	subne	r3, r3, r1		@ non-zero?, decrement set #
	subeq	r3, r3, r2		@ zero?, decrement way # and restore set count
	b	1b

.Ldone_l1inv:
	dsb
	mov	r0, #0			@ default back to cache level 0
	mcr	p15, 2, r0, c0, c0, 0	@ select cache level
	dsb
	isb
	bx	lr
END(armv7_dcache_l1inv_all)

/* * LINTSTUB: void armv7_dcache_inv_all(void); */
ENTRY_NP(armv7_dcache_inv_all)
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	tst	r0, #0x07000000
	beq	.Ldone_inv
	mov	r3, #0			@ start with L1

.Lstart_inv:
	add	r2, r3, r3, lsr #1	@ r2 = level * 3 / 2
	mov	r1, r0, lsr r2		@ r1 = cache type
	tst	r1, #6			@ is it data or i&d?
	beq	.Lnext_level_inv	@ nope, skip level

	mcr	p15, 2, r3, c0, c0, 0	@ select cache level
	isb
	mrc	p15, 1, r0, c0, c0, 0	@ read CCSIDR

	ubfx	ip, r0, #0, #3		@ get linesize from CCSIDR
	add	ip, ip, #4		@ apply bias
	ubfx	r2, r0, #13, #15	@ get numsets - 1 from CCSIDR
	lsl	r2, r2, ip		@ shift to set position
	orr	r3, r3, r2		@ merge set into way/set/level
	mov	r1, #1
	lsl	r1, r1, ip		@ r1 = set decr

	ubfx	ip, r0, #3, #10		@ get numways - 1 from [to be discarded] CCSIDR
	clz	r2, ip			@ number of bits to MSB of way
	lsl	ip, ip, r2		@ shift by that into way position
	mov	r0, #1			@
	lsl	r2, r0, r2		@ r2 now contains the way decr
	mov	r0, r3 			@ get sets/level (no way yet)
	orr	r3, r3, ip		@ merge way into way/set/level
	bfc	r0, #0, #4		@ clear low 4 bits (level) to get numset - 1
	sub	r2, r2, r0		@ subtract from way decr

	/* r3 = ways/sets/level, r2 = way decr, r1 = set decr, r0 and ip are free */
1:	mcr	p15, 0, r3, c7, c6, 2	@ invalidate line
	cmp	r3, #15			@ are we done with this level (way/set == 0)
	bls	.Lnext_level_inv	@ yes, go to next level
	ubfx	r0, r3, #4, #18		@ extract set bits
	cmp	r0, #0			@ compare
	subne	r3, r3, r1		@ non-zero?, decrement set #
	subeq	r3, r3, r2		@ zero?, decrement way # and restore set count
	b	1b

.Lnext_level_inv:
	dsb
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	ubfx	ip, r0, #24, #3		@ narrow to LoC
	add	r3, r3, #2		@ go to next level
	cmp	r3, ip, lsl #1		@ compare
	blt	.Lstart_inv		@ not done, next level (r0 == CLIDR)

.Ldone_inv:
	mov	r0, #0			@ default back to cache level 0
	mcr	p15, 2, r0, c0, c0, 0	@ select cache level
	dsb
	isb
	bx	lr
END(armv7_dcache_inv_all)

/* * LINTSTUB: void armv7_dcache_wbinv_all(void); */
ENTRY_NP(armv7_dcache_wbinv_all)
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	tst	r0, #0x07000000
	bxeq	lr
	mov	r3, #0			@ start with L1

.Lstart_wbinv:
	add	r2, r3, r3, lsr #1	@ r2 = level * 3 / 2
	mov	r1, r0, lsr r2		@ r1 = cache type
	tst	r1, #6			@ is it unified or data?
	beq	.Lnext_level_wbinv	@ nope, skip level

	mcr	p15, 2, r3, c0, c0, 0	@ select cache level
	isb
	mrc	p15, 1, r0, c0, c0, 0	@ read CCSIDR

	ubfx	ip, r0, #0, #3		@ get linesize from CCSIDR
	add	ip, ip, #4		@ apply bias
	ubfx	r2, r0, #13, #15	@ get numsets - 1 from CCSIDR
	lsl	r2, r2, ip		@ shift to set position
	orr	r3, r3, r2		@ merge set into way/set/level
	mov	r1, #1
	lsl	r1, r1, ip		@ r1 = set decr

	ubfx	ip, r0, #3, #10		@ get numways - 1 from [to be discarded] CCSIDR
	clz	r2, ip			@ number of bits to MSB of way
	lsl	ip, ip, r2		@ shift by that into way position
	mov	r0, #1			@
	lsl	r2, r0, r2		@ r2 now contains the way decr
	mov	r0, r3 			@ get sets/level (no way yet)
	orr	r3, r3, ip		@ merge way into way/set/level
	bfc	r0, #0, #4		@ clear low 4 bits (level) to get numset - 1
	sub	r2, r2, r0		@ subtract from way decr

	/* r3 = ways/sets/level, r2 = way decr, r1 = set decr, r0 and ip are free */
1:	mcr	p15, 0, r3, c7, c14, 2	@ writeback and invalidate line
	cmp	r3, #15			@ are we done with this level (way/set == 0)
	bls	.Lnext_level_wbinv	@ yes, go to next level
	ubfx	r0, r3, #4, #18		@ extract set bits
	cmp	r0, #0			@ compare
	subne	r3, r3, r1		@ non-zero?, decrement set #
	subeq	r3, r3, r2		@ zero?, decrement way # and restore set count
	b	1b

.Lnext_level_wbinv:
	dsb
	mrc	p15, 1, r0, c0, c0, 1	@ read CLIDR
	ubfx	ip, r0, #24, #3		@ narrow to LoC
	add	r3, r3, #2		@ go to next level
	cmp	r3, ip, lsl #1		@ compare
	blt	.Lstart_wbinv		@ not done, next level (r0 == CLIDR)

.Ldone_wbinv:
	mov	r0, #0			@ default back to cache level 0
	mcr	p15, 2, r0, c0, c0, 0	@ select cache level
	dsb
	isb
	bx	lr
END(armv7_dcache_wbinv_all)
