/*	$NetBSD: strncmp.S,v 1.7 2013/09/07 19:06:29 chs Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Hiroshi Horimoto <horimoto@cs-aoi.cs.sist.ac.jp> and
 * by J.T. Conklin <jtc@NetBSD.org>.
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
	RCSID("$NetBSD: strncmp.S,v 1.7 2013/09/07 19:06:29 chs Exp $")
#endif /* LIBC_SCCS and not lint */


#ifdef __mcoldfire__
#define GETC(a,b)	mvsb	a,b
#define	SUBC(a,b)	mvsb	a,%d2; subl %d2,b
#else
#define GETC(a,b)	movb	a,b
#define	SUBC(a,b)	subb	a,b
#endif

ENTRY(strncmp)
	movl	12(%sp),%d0
	jeq	.L4
	movl	4(%sp),%a0
	movl	8(%sp),%a1
#ifdef __coldfire__
	movl	%d2,-(%sp)	| save temp
#endif
.L1:				/* unroll by 4 for m680[23]0's */
	GETC((%a0)+,%d1)
	jeq	.L2
	SUBC((%a1)+,%d1)
	jne	.L3
	subql	#1,%d0
	jeq	.L4

	GETC((%a0)+,%d1)
	jeq	.L2
	SUBC((%a1)+,%d1)
	jne	.L3
	subql	#1,%d0
	jeq	.L4

	GETC((%a0)+,%d1)
	jeq	.L2
	SUBC((%a1)+,%d1)
	jne	.L3
	subql	#1,%d0
	jeq	.L4

	GETC((%a0)+,%d1)
	jeq	.L2
	SUBC((%a1)+,%d1)
	jne	.L3
	subql	#1,%d0
	jne	.L1
#ifdef __mcoldfire__
	movl	(%sp)+,%d2		| restore temp
#endif
.L4:	rts

.L2:	SUBC((%a1),%d1)
.L3:	
#ifdef __mcoldfire__
	movl	(%sp)+,%d2		| restore temp
	movl	%d1,%d0
#else
	scs	%d0
	EXTBL(%d0)
	moveb	%d1,%d0
#endif
	rts
END(strncmp)
