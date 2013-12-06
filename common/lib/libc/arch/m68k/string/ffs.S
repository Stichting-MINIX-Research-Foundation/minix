/*	$NetBSD: ffs.S,v 1.6 2013/09/07 19:06:29 chs Exp $	*/

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
	RCSID("from: @(#)ffs.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: ffs.S,v 1.6 2013/09/07 19:06:29 chs Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/* bit = ffs(value) */

WEAK_ALIAS(__ffssi2,ffs)

#if (!defined(__mc68010__) && !defined(__mcoldfire__)) || defined(__mcfisac__)

ENTRY(ffs)
	movl	4(%sp),%d0
	movl	%d0,%d1
	negl	%d0
	andl	%d0,%d1
	movql	#32,%d0
#if defined(__mcfisac__)
	ff1.l	%d1		| count leading zeros
#else
	bfffo	%d1{#0:#32},%d1
#endif
	subl	%d1,%d0
	rts
END(ffs)

#elif defined(__mcoldfire__)

ENTRY(ffs)
	clrl	%d0
	movl	4(%sp),%d1
	jeq	.L2		| return 0 if 0
.L1:
	addql	#1,%d0
	lsrl	#1,%d1		| shift low bit into carry
	jcc	.L1		| keep looping while carry is clear.
.L2:
	rts

#else	/* __mc68010__ */

ENTRY(ffs)
	movl	4(%sp),%d0
	jeq	.L2
	movql	#31,%d1
.L1:	
	lsrl	#1,%d0
	dbcs	%d1,.L1
	movql	#32,%d0
	subl	%d1,%d0
.L2:
	rts
END(ffs)

#endif	/* __mc68010__ */
