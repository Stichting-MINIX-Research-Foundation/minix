/* $NetBSD: crti.S,v 1.3 2015/05/01 21:35:56 matt Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#include <mips/asm.h>

RCSID("$NetBSD: crti.S,v 1.3 2015/05/01 21:35:56 matt Exp $")

#include "sysident.S"

	.section ".init", "ax", @progbits
	.p2align 2
NESTED_NOPROFILE(_init, CALLFRAME_SIZ, ra)
        SETUP_GP
        PTR_SUBU sp, sp, CALLFRAME_SIZ	# allocate stack frame
	REG_S	ra, CALLFRAME_RA(sp)	# save RA
#ifdef __mips_o32
	REG_S	s0, CALLFRAME_S0(sp)	# save s0
	move	s0, gp 
#endif
        SETUP_GP64(CALLFRAME_GP, _init)
END(_init)

	.section ".fini", "ax", @progbits
	.p2align 2
NESTED_NOPROFILE(_fini, CALLFRAME_SIZ, ra)
        SETUP_GP
        PTR_SUBU sp, sp, CALLFRAME_SIZ	# allocate stack frame
	REG_S	ra, CALLFRAME_RA(sp)	# save RA
#ifdef __mips_o32
	REG_S	s0, CALLFRAME_S0(sp)	# save s0
	move	s0, gp 
#endif
        SETUP_GP64(CALLFRAME_GP, _fini)
END(_fini)
