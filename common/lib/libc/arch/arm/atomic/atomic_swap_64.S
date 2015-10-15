/*	$NetBSD: atomic_swap_64.S,v 1.10 2015/05/17 20:57:11 justin Exp $	*/
/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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

#include "atomic_op_asm.h"

ENTRY_NP(_atomic_swap_64)
	push	{r3,r4}			/* save temporary */
	mov	ip, r0			/* return value will be in r0 */
#ifndef __ARM_EABI__
	mov	r3, r2			/* r2 will be overwriten by r1 */
	mov	r2, r1			/* and r1 will be overwritten by ldrexd */
#endif
1:	ldrexd	r0, r1, [ip]		/* load old value */
	strexd	r4, r2, r3, [ip]	/* store new value */
	cmp	r4, #0			/*   succeed? */
	bne	1b			/*    no, try again */
#ifdef _ARM_ARCH_7
	dmb
#else
	mcr	p15, 0, r4, c7, c10, 5	/* data memory barrier */
#endif
	pop	{r3,r4}			/* restore temporary */
	RET
END(_atomic_swap_64)
ATOMIC_OP_ALIAS(atomic_swap_64,_atomic_swap_64)
CRT_ALIAS(__sync_lock_test_and_set_8,_atomic_swap_64)
CRT_ALIAS(__atomic_exchange_8,_atomic_swap_64)

#if (!defined(_KERNEL) || !defined(_RUMPKERNEL)) && !defined(_STANDALONE)
ENTRY_NP(__sync_lock_release_8)
	mov	r2, #0
	mov	r3, #0
	strd	r2, r3, [r0]
	RET
END(__sync_lock_release_8)
#endif
