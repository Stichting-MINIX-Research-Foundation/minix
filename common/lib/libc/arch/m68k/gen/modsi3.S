/*	$NetBSD: modsi3.S,v 1.3 2006/01/13 16:07:59 christos Exp $	*/

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
	RCSID("from: @(#)modsi3.s	5.1 (Berkeley) 6/7/90")
#else
	RCSID("$NetBSD: modsi3.S,v 1.3 2006/01/13 16:07:59 christos Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/* int % int */
ENTRY(__modsi3)
#ifndef __mc68010__
	movel	%sp@(4),%d1
	divsll	%sp@(8),%d0:%d1
	rts
#else
| NB: this requires that __udivsi3 preserve %a0 and return
| the modulus in %d1:
	movel	%sp@+, %a0	| pop return address
	movel	%sp@(4), %d1	| load the divisor
	bpl	1f
	negl	%sp@(4)		| store abs(divisor)
1:	movel	%sp@, %d0	| load the dividend
	pea	%pc@(Lret)	| push our return address
	bpl	2f
	negl	%sp@(4)		| store abs(dividend)
	subql	#2, %sp@	| adjust return address
2:	jmp	_C_LABEL(__udivsi3)
	negl	%d1		| negate modulus
Lret:	movl	%d1, %d0	| move modulus into %d0
	jmp	%a0@
#endif /* __mc68010__ */
