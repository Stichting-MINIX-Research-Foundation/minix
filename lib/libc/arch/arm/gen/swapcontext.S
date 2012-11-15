/*	$NetBSD: swapcontext.S,v 1.8 2012/09/27 11:20:20 skrll Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
        RCSID("$NetBSD: swapcontext.S,v 1.8 2012/09/27 11:20:20 skrll Exp $")
#endif /* LIBC_SCCS && !lint */

ENTRY(swapcontext)
	stmfd	sp!, {r0-r1, lr}	/* Must save oucp, ucp, lr. */
	sub	sp, #4
	bl	PIC_SYM(_C_LABEL(_getcontext), PLT)  /* getcontext(oucp) */
	cmp	r0, #0
	add	sp, #4
	ldmfd	sp!, {r0-r1, lr}
	RETc(ne)
	str	sp, [r0, #_UC_REGS_SP]	/* Adjust saved SP. */
	str	lr, [r0, #_UC_REGS_PC]	/* Adjust saved PC. */
#ifdef SOFTFLOAT
	/* Ahem. */
#endif
	mov	r0, r1
	bl	PIC_SYM(_C_LABEL(setcontext), PLT)
	RET
