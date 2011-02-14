/*	$NetBSD: swapcontext.S,v 1.4 2009/12/14 01:07:42 matt Exp $	*/

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

#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: swapcontext.S,v 1.4 2009/12/14 01:07:42 matt Exp $")
#endif /* SYSLIBC_SCCS && !lint */

	.set	reorder
	
NESTED(swapcontext, CALLFRAME_SIZ, ra)
	.mask	0x80000030, (CALLFRAME_RA - CALLFRAME_SIZ)
	PTR_SUBU	sp, sp, CALLFRAME_SIZ
	PTR_S		ra, CALLFRAME_RA(sp)	# save ra
	PTR_S		a0, 0(sp)		# stash away oucp
	PTR_S		a1, SZREG(sp)		# stash away ucp
	SYSTRAP(getcontext)
	PTR_L		ra, CALLFRAME_RA(sp)
	bne		a3, zero, 1f

	PTR_L		v1, 0(sp)		# load oucp again for adjustment
	PTR_L		a0, SZREG(sp)		# load ucp
	PTR_ADDIU	v0, sp, CALLFRAME_SIZ
	REG_PROLOGUE
	REG_S		zero, _OFFSETOF_UC_GREGS_V0(v1)
	REG_S		ra, _OFFSETOF_UC_GREGS_EPC(v1)
	REG_S		v0, _OFFSETOF_UC_GREGS_SP(v1)
	REG_EPILOGUE

	SYSTRAP(setcontext)
	/* NOTREACHED */
1:
	PTR_ADDU	sp, sp, CALLFRAME_SIZ
	SETUP_GPX(t0);
	SETUP_GP64(t3,swapcontext);
	PIC_TAILCALL(__cerror)
END(swapcontext)
