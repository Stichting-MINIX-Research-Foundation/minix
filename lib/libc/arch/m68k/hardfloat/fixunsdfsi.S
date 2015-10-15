/*	$NetBSD: fixunsdfsi.S,v 1.3 2014/03/18 18:20:37 riastradh Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)fixunsdfsi.s	5.1 (Berkeley) 6/7/90")
#else
	RCSID("$NetBSD: fixunsdfsi.S,v 1.3 2014/03/18 18:20:37 riastradh Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

#ifdef __mcoldfire__
	.section .rodata,"a"
	.p2align 4
L2G:	.double 0r2147483648.0
#endif

/* (unsigned) double */
ENTRY(__fixunsdfsi)
	fintrzd	4(%sp),%fp0
#ifdef __mcoldfire__
	LEA_LCL(L2G,%a0)
	fmoved	(%a0),%fp1
	fcmpd	%fp1,%fp0
#else
	fmoved	#0r2147483648.0,%fp1
	fcmpx	%fp1,%fp0
#endif
	fbge	Lwaybig
	fmovel	%fp0,%d0
	rts
Lwaybig:
#ifdef __mcoldfire__
	fsubd	%fp1,%fp0
#else
	fsubx	%fp1,%fp0
#endif
	fmovel	%fp0,%d0
	bset	#31,%d0
	rts
END(__fixunsdfsi)
