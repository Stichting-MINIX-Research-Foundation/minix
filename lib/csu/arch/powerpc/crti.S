/* $NetBSD: crti.S,v 1.4 2014/03/06 19:05:59 matt Exp $ */

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

#include <powerpc/asm.h>

RCSID("$NetBSD: crti.S,v 1.4 2014/03/06 19:05:59 matt Exp $")

#include "sysident.S"

	.section ".init", "ax", @progbits
_XENTRY(_init)
	mflr	%r0
#ifdef _LP64
	stdu	%r1, -(6*SZREG)(%r1)
	std	%r0, (2*SZREG)(%r1)
#else
	stw	%r0, (1*SZREG)(%r1)
	stwu	%r1, -(4*SZREG)(%r1)
#endif

	.section ".fini", "ax", @progbits
_XENTRY(_fini)
	mflr	%r0
#ifdef _LP64
	stdu	%r1, -(6*SZREG)(%r1)
	std	%r0, (2*SZREG)(%r1)
#else
	stw	%r0, (1*SZREG)(%r1)
	stwu	%r1, -(4*SZREG)(%r1)
#endif
