/*	$NetBSD: atomic_cas.S,v 1.13 2015/07/10 20:16:01 mlelstv Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

	.text

ENTRY(_atomic_cas_32)
	movl	4(%sp), %a0
	movl	8(%sp), %d0
	movl	12(%sp), %d1
	casl	%d0, %d1, (%a0)
	/* %d0 now contains the old value */
	movl	%d0, %a0	/* pointers return also in %a0 */
	rts
END(_atomic_cas_32)

ATOMIC_OP_ALIAS(atomic_cas_32,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ulong,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ptr,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ptr,_atomic_cas_32)
CRT_ALIAS(__sync_val_compare_and_swap_4,_atomic_cas_32)

ATOMIC_OP_ALIAS(atomic_cas_32_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_32_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_uint_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_uint_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ulong_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong_ni,_atomic_cas_32)
ATOMIC_OP_ALIAS(atomic_cas_ptr_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ptr_ni,_atomic_cas_32)

ENTRY(__sync_bool_compare_and_swap_4)
	movl	4(%sp), %a0
	movl	8(%sp), %d0
	movl	12(%sp), %d1
	casl	%d0, %d1, (%a0)
	beqs	1f
	clrl	%d0	/* return false */
	rts
1:	movql	#1, %d0	/* return success */
	rts
END(__sync_bool_compare_and_swap_4)

ENTRY(_atomic_cas_16)
	movl	4(%sp), %a0
	movw	8+2(%sp), %d0		/* lower word */
	movw	12+2(%sp), %d1		/* lower word */
	casw	%d0, %d1, (%a0)
	/* %d0 now contains the old value */
	rts
END(_atomic_cas_16)

ATOMIC_OP_ALIAS(atomic_cas_16,_atomic_cas_16)
CRT_ALIAS(__sync_val_compare_and_swap_2,_atomic_cas_16)

ENTRY(__sync_bool_compare_and_swap_2)
	movl	4(%sp), %a0
	movw	8+2(%sp), %d0		/* lower word */
	movw	12+2(%sp), %d1		/* lower word */
	casw	%d0, %d1, (%a0)
	/* %d0 now contains the old value */
	beqs	1f
	clrl	%d0	/* return failure */
	rts
1:	movql	#1, %d0	/* return success */
	rts
END(__sync_bool_compare_and_swap_2)

ENTRY(_atomic_cas_8)
	movl	4(%sp), %a0
	movb	8+3(%sp), %d0		/* lower byte */
	movb	12+3(%sp), %d1		/* lower byte */
	casb	%d0, %d1, (%a0)
	/* %d0 now contains the old value */
	rts
END(_atomic_cas_8)

ATOMIC_OP_ALIAS(atomic_cas_8,_atomic_cas_8)
CRT_ALIAS(__sync_val_compare_and_swap_1,_atomic_cas_8)


ENTRY(__sync_bool_compare_and_swap_1)
	movl	4(%sp), %a0
	movb	8+3(%sp), %d0		/* lower byte */
	movb	12+3(%sp), %d1		/* lower byte */
	casb	%d0, %d1, (%a0)
	/* %d0 now contains the old value */
	beqs	1f
	clrl	%d0	/* return failure */
	rts
1:	movql	#1, %d0	/* return success */
	rts
END(__sync_bool_compare_and_swap_1)

