/*	$NetBSD: crt0.S,v 1.1 2012/08/13 02:49:04 matt Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include <arm/asm.h>

RCSID("$NetBSD: crt0.S,v 1.1 2012/08/13 02:49:04 matt Exp $")

STRONG_ALIAS(_start,__start)

_ENTRY(__start)
#ifdef __minix
	mov	r3, r2		/* cleanup */
	mov	r4, r1		/* obj_main */
	and	r5, r5, #0	/* ps_strings, always NULL on MINIX */

	/* Get argc, argv, and envp from stack */
	ldr	r0, [sp, #0]	/* get argc */
	add	r1, sp, #4	/* argv = sp + 4 */
	add	r2, r1, r0, lsl #2	/* envp = argv + argc*4 */
	add	r2, r2, #4	/* skip NULL terminator */
#else
	/*
	 * We need to swap ps_strings and cleanup
 	 */
	mov	ip, r0		/* ps_strings -> tmp */
	mov	r0, r2		/* cleanup -> ps_strings */
	mov	r2, ip		/* tmp -> ps_strings */
#endif /* __minix */
	/* Ensure the stack is properly aligned before calling C code. */
	bic	sp, sp, #7

#ifdef __minix
	/* Store obj and ps_strings on the stack */
	sub	sp, sp, #8
	str	r5, [sp, #4]
	str	r4, [sp, #0]
#endif
	/*
	 * void ___start(void (*cleanup)(void),
	 *    const Obj_Entry *obj,
	 *    struct ps_strings *ps_strings);
	 */

	b	___start
