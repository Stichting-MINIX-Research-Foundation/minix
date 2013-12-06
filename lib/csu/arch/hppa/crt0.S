/*	$NetBSD: crt0.S,v 1.1 2013/07/13 18:52:35 skrll Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nick Hudson
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

RCSID("$NetBSD: crt0.S,v 1.1 2013/07/13 18:52:35 skrll Exp $")

	.import	_GLOBAL_OFFSET_TABLE_
	.hidden	___start

STRONG_ALIAS(__start,_start)

/*
 * void _start(struct ps_strings *ps_strings,
 *    void (*cleanup)(void),
 *    const Obj_Entry *obj);
 */
_ENTRY(_start)
	.callinfo frame=0, calls
	.entry

	bl      L$lpc, %r27
	depi    0, 31, 2, %r27
L$lpc:	addil   L'_GLOBAL_OFFSET_TABLE_ - ($PIC_pcrel$0 - 8), %r27
	ldo     R'_GLOBAL_OFFSET_TABLE_ - ($PIC_pcrel$0 - 12)(%r1),%r27
	copy	%r27, %r19

	/*
	 * void ___start(void (*cleanup)(void),
	 *    const Obj_Entry *obj,
	 *    struct ps_strings *ps_strings);
	 */
	copy	%arg0, %arg3		; ps_strings -> tmp
	copy	%arg1, %arg0		; cleanup -> arg0
	copy	%arg2, %arg1		; obj -> arg1
	b	___start
	copy	%arg3, %arg2		; ps_strings (tmp) -> arg2

EXIT(_start)
