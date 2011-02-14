/*	$NetBSD: strncmp.S,v 1.2 2008/04/28 20:22:52 martin Exp $	*/

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
	RCSID("$NetBSD: strncmp.S,v 1.2 2008/04/28 20:22:52 martin Exp $")
#endif /* LIBC_SCCS and not lint */

ENTRY(strncmp)
	movl	%sp@(12),%d0
	jeq	L4
	movl	%sp@(4),%a0
	movl	%sp@(8),%a1
L1:				/* unroll by 4 for m680[23]0's */
	movb	%a0@+,%d1
	jeq	L2
	subb	%a1@+,%d1
	jne	L3
	subql	#1,%d0
	jeq	L4

	movb	%a0@+,%d1
	jeq	L2
	subb	%a1@+,%d1
	jne	L3
	subql	#1,%d0
	jeq	L4

	movb	%a0@+,%d1
	jeq	L2
	subb	%a1@+,%d1
	jne	L3
	subql	#1,%d0
	jeq	L4

	movb	%a0@+,%d1
	jeq	L2
	subb	%a1@+,%d1
	jne	L3
	subql	#1,%d0
	jne	L1
L4:	rts

L2:	subb	%a1@,%d1
L3:	scs	%d0
	EXTBL(%d0)
	movb	%d1,%d0
	rts
