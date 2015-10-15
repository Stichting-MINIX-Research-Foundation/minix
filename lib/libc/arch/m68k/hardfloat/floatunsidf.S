/*	$NetBSD: floatunsidf.S,v 1.2 2014/03/18 18:20:37 riastradh Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
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

#if defined(LIBC_SCCS) && !defined(lint)
RCSID("$NetBSD: floatunsidf.S,v 1.2 2014/03/18 18:20:37 riastradh Exp $")
#endif /* LIBC_SCCS and not lint */

#ifdef __mcoldfire__
	.section .rodata,"a"
	.p2align 2
L2G:	.double 0r2147483648.0
#endif

/* LINTSTUB: double __floatunsidf(unsigned int); */
ENTRY(__floatunsidf)
	movl	4(%sp),%d0
	jpl	1f
	fmovel	%d0,%fp0
#ifdef __SVR4_ABI__
	rts
#else
	jra	Lret
#endif
1:
	bclr	#31,%d0
	fmovel	%d0,%fp0
#ifdef __mcoldfire__
	LEA_LCL(L2G,%a0)
	faddd	(%a0),%fp0
#else
	faddd	#0r2147483648.0,%fp0
#endif
#ifndef __SVR4_ABI__
Lret:
	fmoved	%fp0,-(%sp)
	movel	(%sp)+,%d0
	movel	(%sp)+,%d1
#endif
	rts
END(__floatunsidf)
