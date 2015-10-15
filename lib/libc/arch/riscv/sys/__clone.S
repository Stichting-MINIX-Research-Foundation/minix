/*	$NetBSD: __clone.S,v 1.3 2015/03/27 23:23:14 matt Exp $	*/

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

#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: __clone.S,v 1.3 2015/03/27 23:23:14 matt Exp $");
#endif /* SYSLIBC_SCCS and not lint */

#ifdef WEAK_ALIAS
WEAK_ALIAS(clone, __clone)
#endif

/*
 * int __clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)
	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	mv	t0, a0			/* we need a0 for return value */
	li	a0, EINVAL
	beqz	t0, 8f
	beqz	a1, 8f

	/*
	 * We need to be able to get at the func and arg arguments
	 * in the child.  Luckily, we have a convenient place to
	 * do this: the child's stack.
	 */
	addi	a1, a1, -CALLFRAME_SIZ
	REG_S	t0, 0(a1)
	REG_S	a3, SZREG(a1)

	/*
	 * The system call expects (flags, stack).
	 */
	mv	a0, a2
	SYSTRAP(__clone)
	# a1 (rv[1]) == 0, parent, child pid in a0
8:	JUMP_TO_CERROR()	/* error */
	bnez	a1, 9f		/* success */
	ret			# parent return

	/* NOTREACHED */

9:	/*
	 * Child: Fetch the function and argument from the new stack and create
	 a frame so that the child can be safely called.
	 *
	 * There are already register slots on the stack from above.
	 * They already include the o32 argument save area.  The
	 * highest is unused.  a1 should equal sp now.
	 */

	REG_L		t0, 0(sp)
	REG_L		a0, SZREG(sp)

	REG_S		zero, CALLFRAME_RA(sp)	/* make sure stack frame ends */

	/* Call the clone's entry point. */
	jalr		t0

	/* Pass the return value to _exit. */
	tail		_C_LABEL(_exit)

	/* NOTREACHED */
END(__clone)
