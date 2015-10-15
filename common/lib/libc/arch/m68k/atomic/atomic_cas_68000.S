/*	$NetBSD: atomic_cas_68000.S,v 1.6 2014/01/29 10:20:11 martin Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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

#include <sys/ras.h>
#include "atomic_op_asm.h"

	.text

ENTRY(_atomic_cas_up)
	.hidden	_C_LABEL(_atomic_cas_up)

	movl	4(%sp), %a0		/* Fetch ptr */

RAS_START_ASM_HIDDEN(_atomic_cas)
	movl	(%a0), %d0		/* d0 = *ptr */
	cmpl	8(%sp), %d0		/* Same as old? */
	jne	1f			/* Nope */
	movl	12(%sp), (%a0)		/* *ptr = new */
RAS_END_ASM_HIDDEN(_atomic_cas)
1:	rts
END(_atomic_cas_up)

ENTRY(_atomic_cas_16_up)
	.hidden	_C_LABEL(_atomic_cas_16_up)

	movl	4(%sp), %a0		/* Fetch ptr */

RAS_START_ASM_HIDDEN(_atomic_cas_16)
	movw	(%a0), %d0		/* d0 = *ptr */
	cmpw	8(%sp), %d0		/* Same as old? */
	jne	1f			/* Nope */
	movw	12(%sp), (%a0)		/* *ptr = new */
RAS_END_ASM_HIDDEN(_atomic_cas_16)
1:	rts
END(_atomic_cas_16_up)


ENTRY(_atomic_cas_8_up)
	.hidden	_C_LABEL(_atomic_cas_8_up)

	movl	4(%sp), %a0		/* Fetch ptr */

RAS_START_ASM_HIDDEN(_atomic_cas_8)
	movb	(%a0), %d0		/* d0 = *ptr */
	cmpb	8(%sp), %d0		/* Same as old? */
	jne	1f			/* Nope */
	movb	12(%sp), (%a0)		/* *ptr = new */
RAS_END_ASM_HIDDEN(_atomic_cas_8)
1:	rts
END(_atomic_cas_8_up)
