/*	$NetBSD: swapcontext.S,v 1.3 2008/04/28 20:22:55 martin Exp $	*/

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

NESTED(swapcontext, 2, 24, ra, IM_RA|IM_A0|IM_A1, 0)
	LDGP(pv)
	lda	sp, -24(sp)
	stq	ra, (24- 8)(sp)		/* must save ra, oucp, ucp */
	stq	a0, (24-16)(sp)
	stq	a1, (24-24)(sp)
	CALL(_getcontext)		/* getcontext(oucp) */
	ldq	t0, (24-16)(sp)
	bne	v0, Lerr
	ldq	t1, (24- 8)(sp)
	stq	t1, (56 + 26 * 8)(t0)	/* Adjust saved RA */
	stq	t1, (56 + 32 * 8)(t0)	/* Adjust saved PC */
	lda	t1, 24(sp)
	stq	t1, (56 + 30 * 8)(t0)	/* Adjust saved SP */

	ldq	a0, (24-24)(sp)
	CALL(setcontext)		/* setcontext(ucp) */
Lerr:
	ldq	ra, (24- 8)(sp)
	lda	sp, 24(sp)
	RET
END(swapcontext)
