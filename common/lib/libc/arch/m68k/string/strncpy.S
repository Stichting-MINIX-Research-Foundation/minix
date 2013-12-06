/*	$NetBSD: strncpy.S,v 1.5 2013/09/07 19:06:29 chs Exp $	*/

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
	RCSID("from: @(#)strncpy.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: strncpy.S,v 1.5 2013/09/07 19:06:29 chs Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

ENTRY(strncpy)
	movl	4(%sp),%d0		| return value is toaddr
	movl	12(%sp),%d1		| count
	jeq	.Lscdone		| nothing to do
	movl	8(%sp),%a0		| a0 = fromaddr
	movl	%d0,%a1			| a1 = toaddr
	subql	#1,%d1
.Lscloop:
	movb	(%a0)+,(%a1)+		| copy a byte
#ifndef __mcoldfire__
	dbeq	%d1,.Lscloop		| loop through low word of d1
#endif
	jeq	.Lscpadding		| copied null, padding if necessary
#ifndef __mcoldfire__
	clrw	%d1			| clear low word of d1
#endif
	subql	#1,%d1			| adjust count for long copies
	jcc	.Lscloop		| more room, keep going
.Lscdone:
#ifdef __SVR4_ABI__
	moveal	%d0,%a0
#endif
	rts

.Lscploop:
	clrb	(%a1)+			| clear a byte
#ifndef __mcoldfire__
	dbra	%d1,.Lscploop		| keep going
	clrw	%d1
#endif
.Lscpadding:
	subql	#1,%d1
	jcc	.Lscploop
	jra	.Lscdone
END(strncpy)
