/*	$NetBSD: __clone.S,v 1.3 2008/04/28 20:22:55 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * int __clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
LEAF(__clone, 4)
	br	pv, 1f
1:	LDGP(pv)
	
	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	beq	a0, 9f
	beq	a1, 9f

	/*
	 * We need to be able to get at the func and arg arguments
	 * in the child.  Luckily, we have a convenient place to
	 * do this; the child's stack.
	 */
	subq	a1, 16, a1
	stq	a0, 0(a1)
	stq	a3, 8(a1)

	/*
	 * The system call expects (flags, stack).
	 */
	mov	a2, a0
	CALLSYS_ERROR(__clone)

	beq	a4, 8f		/* a4 (rv[1]) == 0, parent, child pid in v0 */

	/*
	 * Fetch the function and argument from the new stack, and
	 * pop it back.
	 */
	ldq	pv, 0(sp)
	ldq	a0, 8(sp)
	addq	sp, 16, sp

	/* Call the clone's entry point. */
	CALL((pv))

	/* Pass return value to _exit(). */
	mov	v0, a0
	CALL(_exit)

	/* NOTREACHED */

8:	RET

9:	ldiq	v0, EINVAL
	jmp	zero, __cerror
	END(__clone)
