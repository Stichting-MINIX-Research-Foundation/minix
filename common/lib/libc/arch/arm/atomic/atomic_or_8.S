/*	$NetBSD: atomic_or_8.S,v 1.3 2014/06/23 21:53:45 joerg Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>
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

#ifdef _ARM_ARCH_6

ENTRY_NP(_atomic_or_8)
	mov	ip, r0
1:	ldrexb	r0, [ip]		/* load old value (to be returned) */
	orrs	r3, r0, r1		/* calculate new value */
	strexb	r2, r3, [ip]		/* try to store */
	cmp	r2, #0			/*   succeed? */
	bne	1b			/*     no, try again */
#ifdef _ARM_ARCH_7
	dmb
#else
	mcr	p15, 0, r2, c7, c10, 5	/* data memory barrier */
#endif
	RET				/* return old value */
END(_atomic_or_8)

ATOMIC_OP_ALIAS(atomic_or_8,_atomic_or_8)
ATOMIC_OP_ALIAS(atomic_or_char,_atomic_or_8)
CRT_ALIAS(__sync_fetch_and_or_1,_atomic_or_8)
CRT_ALIAS(__atomic_fetch_or_1,_atomic_or_8)
STRONG_ALIAS(_atomic_or_char,_atomic_or_8)

ENTRY_NP(_atomic_or_8_nv)
	mov	ip, r0			/* need r0 for return value */
1:	ldrexb	r0, [ip]		/* load old value */
	orrs	r0, r0, r1		/* calculate new value (return value) */
	strexb	r2, r0, [ip]		/* try to store */
	cmp	r2, #0			/*   succeed? */
	bne	1b			/*     no, try again? */
#ifdef _ARM_ARCH_7
	dmb
#else
	mcr	p15, 0, r2, c7, c10, 5	/* data memory barrier */
#endif
	RET				/* return new value */
END(_atomic_or_8_nv)

ATOMIC_OP_ALIAS(atomic_or_8_nv,_atomic_or_8_nv)
ATOMIC_OP_ALIAS(atomic_or_char_nv,_atomic_or_8_nv)
CRT_ALIAS(__sync_or_and_fetch_1,_atomic_or_8_nv)
STRONG_ALIAS(_atomic_or_char_nv,_atomic_or_8_nv)

#endif /* _ARM_ARCH_6 */
