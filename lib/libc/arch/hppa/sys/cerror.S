/*	$NetBSD: cerror.S,v 1.5 2013/09/12 15:36:15 joerg Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nick Hudson
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

#include "SYS.h"

#ifdef _REENTRANT
ENTRY(__cerror,HPPA_FRAME_SIZE)
#else
LEAF_ENTRY(__cerror)
#endif
#ifdef _REENTRANT
	stw	%rp, HPPA_FRAME_CRP(%sp)
	stw	%t1, HPPA_FRAME_ARG(0)(%sp)
	ldo	HPPA_FRAME_SIZE(%sp),%sp
	bl	__errno, %rp
	nop
	ldo	-HPPA_FRAME_SIZE(%sp), %sp
	ldw	HPPA_FRAME_ARG(0)(%sp), %t1
	ldw	HPPA_FRAME_CRP(%sp), %rp
	stw	%t1, 0(%ret0)
#else
#ifdef __PIC__
	addil	LT%errno, %r19
	ldw	RT%errno(%r1), %r1
	stw	%t1, 0(%r1)
#else
	ldil	L%errno, %r1
	stw	%t1, R%errno(%r1)
#endif
#endif
	ldi	-1, %ret0
	bv	%r0(%rp)
	ldi	-1, %ret1
EXIT(__cerror)
