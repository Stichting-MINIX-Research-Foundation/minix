/*	$NetBSD: mulsi3.S,v 1.2 2006/01/05 22:33:34 he Exp $	*/

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
	RCSID("from: @(#)mulsi3.s	5.1 (Berkeley) 6/7/90")
#else
	RCSID("$NetBSD: mulsi3.S,v 1.2 2006/01/05 22:33:34 he Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/* int * int */
ENTRY(__mulsi3)
#ifndef __mc68010__
	movel	%sp@(4),%d0
	mulsl	%sp@(8),%d0
#else
| NB: this requires that __udivsi3 preserve %a0 and return
| the modulus in %d1:
	movew	%sp@(6), %d0
	movel	%d0, %a0	| save B
	muluw	%sp@(8), %d0	| %d0 holds B * C
	movew	%sp@(10), %d1
	movel	%d1, %a1	| save D
	muluw	%sp@(4), %d1	| %d1 holds A * D
	addw	%d1, %d0	| %d0 holds (B * C) + (A * D)
	swap	%d0
	clrw	%d0		| %d0 holds ((B * C) + (A * D)) << 16
	exg	%a0, %d0	| restore B
	movel	%a1, %d1	| restore D
	muluw	%d1, %d0	| %d0 holds B * D
	addl	%a0, %d0	| final result
#endif /* __mc68010__ */
	rts
