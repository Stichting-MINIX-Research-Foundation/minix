/* $NetBSD: crti.S,v 1.1 2014/03/14 18:15:02 cherry Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Cherry G. Mathew of Backyard Innovations, UK, Ltd.
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

#include <machine/asm.h>

RCSID("$NetBSD: crti.S,v 1.1 2014/03/14 18:15:02 cherry Exp $")

#include "sysident.S"

	.section ".init", "ax", @progbits
	.align 64
	.proc _init
	.global _init
_init:
	.regstk	0, 2, 0, 0
	.prologue 12, loc0
	.save ar.pfs, loc1
	alloc	loc1 = ar.pfs, 0, 2, 0, 0
	mov	loc0 = b0 /* return addr */
	.endp _init
	
	.section ".fini", "ax", @progbits
	.align 64
	.proc _fini
	.global _fini
_fini:
	.regstk	0, 2, 0, 0
	.prologue 12, loc0
	.save ar.pfs, loc1
	alloc	loc1 = ar.pfs, 0, 2, 0, 0
	mov	loc0 = b0 /* return addr */
	.endp _fini


