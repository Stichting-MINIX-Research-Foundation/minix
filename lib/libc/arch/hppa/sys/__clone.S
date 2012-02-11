/*	$NetBSD: __clone.S,v 1.6 2008/04/28 20:22:56 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Fredette.
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

#include <sys/errno.h>
#include "SYS.h"

#ifdef WEAK_ALIAS
	WEAK_ALIAS(clone, __clone)
#endif

	.import	__cerror, code

/*
 * int clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone, 0)

	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	ldi	EINVAL, %t1
	comb,=	%r0, %arg0, __cerror
	nop
	comb,=	%r0, %arg1, __cerror
	nop

	/*
	 * Put the func and arg arguments into a frame in the child's stack.
	 */
	ldo	(HPPA_FRAME_SIZE * 2)(%arg1), %arg1
	stw	%arg0, HPPA_FRAME_ARG(0)(%arg1)
	stw	%arg3, HPPA_FRAME_ARG(1)(%arg1)

	/*
	 * The system call expects (flags, stack).
	 */
	copy	%arg2, %arg0
	SYSCALL(__clone)
	comb,<>,n %r0, %ret1, 9f
	bv,n	%r0(%rp)

9:	/*
	 * Child: Reload the function and argument from the new stack.
	 */
	ldw	HPPA_FRAME_ARG(0)(%sp), %r22
	ldw	HPPA_FRAME_ARG(1)(%sp), %arg0

	/* Call the clone's entry point. */
	stw	%r19, HPPA_FRAME_ARG(2)(%sp)
	bl	$$dyncall, %r31
	copy	%r31, %rp
	ldw	HPPA_FRAME_ARG(2)(%sp), %r19

	/* Pass the return value to _exit(). */
	copy	%ret0, %arg0
	PIC_CALL(_exit)

	/* NOTREACHED */
EXIT(__clone)
