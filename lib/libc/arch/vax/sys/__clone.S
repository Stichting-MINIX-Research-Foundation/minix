/*	$NetBSD: __clone.S,v 1.3 2008/04/28 20:22:58 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>
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
ENTRY(__clone, 0)
	
	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	movl	4(%ap),%r2	/* check and save function */
	beql	9f
	tstl	8(%ap)		/* check stack */
	beql	9f

	/*
	 * The system call expects (flags, stack).
	 */
	movl	12(%ap),4(%ap)	/* XXX this doesn't work for
				   callg with a RO arglist */
	movl	$2,(%ap)
	SYSTRAP(__clone)	/* only %r0/%r1 munged */

	blbc	%r1,8f		/* %r1<0>:   0=parent  1=child */

	/* Call the clone's entry point. */
	pushl	16(%ap)
	calls	$1,(%r2)

	/* Pass return value to _exit(). */
	pushl	%r0
	calls	$1,_C_LABEL(_exit)

	/* NOTREACHED */

8:	ret

9:	movl	$EINVAL,%r0
	jmp	CERROR+2
