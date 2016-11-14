/*	$NetBSD: fusu.S,v 1.3 2013/08/18 06:28:18 matt Exp $	*/

/*
 * Copyright (c) 1996-1998 Mark Brinicombe.
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

#include "opt_multiprocessor.h"
#include "opt_cpuoptions.h"

#include "assym.h"

#include <machine/asm.h>

#include <arm/locore.h>

/*
 * On PROG26 systems (i.e. acorn26) taking a page fault corrupts R14_svc.
 * To prevent this causing a problem, we save R14 in R12 in these functions.
 */
#ifdef __PROG26
#define SAVE_R14	mov	ip, lr
#define FUSU_RET	mov	pc, ip
#else
#define SAVE_R14	/* do nothing */
#define FUSU_RET	mov	pc, lr
#endif
	
/*
 * fuword(void *uaddr);
 * Fetch an int from the user's address space.
 */

ENTRY(fuword)
	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, .Lfusufault
	str	r1, [r2, #PCB_ONFAULT]

	ldrt	r3, [r0]

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	r0, r3
	FUSU_RET
END(fuword)

/*
 * fusword(void *uaddr);
 * Fetch a short from the user's address space.
 */

ENTRY(fusword)
	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, .Lfusufault
	str	r1, [r2, #PCB_ONFAULT]

	ldrbt	r3, [r0], #1
	ldrbt	r1, [r0]
#ifdef __ARMEB__
	orr	r0, r1, r3, asl #8
#else
	orr	r0, r3, r1, asl #8
#endif
	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	FUSU_RET
END(fusword)

/*
 * fuswintr(void *uaddr);
 * Fetch a short from the user's address space.  Can be called during an
 * interrupt.
 */

ENTRY(fuswintr)
	ldr	r2, .Lblock_userspace_access
	ldr	r2, [r2]
	teq	r2, #0
	mvnne	r0, #0x00000000
	movne	pc, lr

	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, _C_LABEL(fusubailout)
	str	r1, [r2, #PCB_ONFAULT]

	ldrbt	r3, [r0], #1
	ldrbt	r1, [r0]
#ifdef __ARMEB__
	orr	r0, ip, r1, asl #8
#else
	orr	r0, r1, ip, asl #8
#endif

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	FUSU_RET
END(fuswintr)

.Lblock_userspace_access:
	.word	_C_LABEL(block_userspace_access)

	.data
	.align	0
	.global	_C_LABEL(block_userspace_access)
_C_LABEL(block_userspace_access):
	.word	0
	.text

/*
 * fubyte(void *uaddr);
 * Fetch a byte from the user's address space.
 */

ENTRY(fubyte)
	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, .Lfusufault
	str	r1, [r2, #PCB_ONFAULT]

	ldrbt	r3, [r0]

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	r0, r3
	FUSU_RET

/*
 * Handle faults from [fs]u*().  Clean up and return -1.
 */

.Lfusufault:
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mvn	r0, #0x00000000
	FUSU_RET
END(fubyte)

/*
 * Handle faults from [fs]u*().  Clean up and return -1.  This differs from
 * fusufault() in that trap() will recognise it and return immediately rather
 * than trying to page fault.
 */

/* label must be global as fault.c references it */
_ENTRY(fusubailout)
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mvn	r0, #0x00000000
	FUSU_RET
END(fusubailout)

#ifdef DIAGNOSTIC
/*
 * Handle earlier faults from [fs]u*(), due to no pcb
 */

.Lfusupcbfault:
	mov	r1, r0
	adr	r0, .Lfusupcbfaulttext
	b	_C_LABEL(panic)

.Lfusupcbfaulttext:
	.asciz	"Yikes - no valid PCB during fusuxxx() addr=%08x\n"
	.align	0
#endif

/*
 * suword(void *uaddr, int x);
 * Store an int in the user's address space.
 */

ENTRY(suword)
	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, .Lfusufault
	str	r3, [r2, #PCB_ONFAULT]

	strt	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	FUSU_RET
END(suword)

/*
 * suswintr(void *uaddr, short x);
 * Store a short in the user's address space.  Can be called during an
 * interrupt.
 */

ENTRY(suswintr)
	ldr	r2, .Lblock_userspace_access
	ldr	r2, [r2]
	teq	r2, #0
	mvnne	r0, #0x00000000
	movne	pc, lr

	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, _C_LABEL(fusubailout)
	str	r3, [r2, #PCB_ONFAULT]

#ifdef __ARMEB__
	mov	r3, r1, lsr #8
	strbt	r3, [r0], #1
#else
	strbt	r1, [r0], #1
	mov	r1, r1, lsr #8
#endif
	strbt	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	FUSU_RET
END(suswintr)

/*
 * susword(void *uaddr, short x);
 * Store a short in the user's address space.
 */

ENTRY(susword)
	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, .Lfusufault
	str	r3, [r2, #PCB_ONFAULT]

#ifdef __ARMEB__
	mov	r3, r1, lsr #8
	strbt	r3, [r0], #1
#else
	strbt	r1, [r0], #1
	mov	r1, r1, lsr #8
#endif
	strbt	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	FUSU_RET
END(susword)

/*
 * subyte(void *uaddr, char x);
 * Store a byte in the user's address space.
 */

ENTRY(subyte)
	SAVE_R14
	GET_CURPCB(r2)

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, .Lfusufault
	str	r3, [r2, #PCB_ONFAULT]

	strbt	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	FUSU_RET
END(subyte)
