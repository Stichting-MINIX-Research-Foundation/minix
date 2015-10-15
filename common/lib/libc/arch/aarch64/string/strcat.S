/* $NetBSD: strcat.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
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

#include <machine/asm.h>

RCSID("$NetBSD: strcat.S,v 1.1 2014/08/10 05:47:35 matt Exp $")

/*
 * Implementation of strcat using strlen/strcpy.
 */
ENTRY(strcat)
	stp	x29, x30, [sp, #-32]!	/* save fp, lr */
	mov	x29, sp			/* init fp */
	stp	x27, x28, [x29, #16]	/* save registers */

	mov	x27, x0			/* save dst */
	mov	x28, x1			/* save append */

	bl	_C_LABEL(strlen)	/* find length of dst */

	add	x0, x0, x27		/* add to dst pointer */
	mov	x1, x28			/* fetch append */
	bl	_C_LABEL(strcpy)	/* append string */

	mov	x0, x27			/* restore dst as return value */
	ldp	x27, x28, [x29, #16]	/* save registers */
	ldp	x29, x30, [sp], #32	/* save fp, lr; pop stack */
	ret
END(strcat)
