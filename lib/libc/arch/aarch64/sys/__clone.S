/* $NetBSD: __clone.S,v 1.1 2014/08/10 05:47:37 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
#include <sys/errno.h>

WEAK_ALIAS(clone, __clone)

/*
 * int __clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)

	/* test stack and function are not null */
	cbz	x0, .Leinval
	cbz	x1, .Leinval

	/* place the arg and func onto the child's stack */
	stp	x3, x0, [x1, #-16]!
	
	/* syscall expects (flags, stack) */
	mov	x0, x2

	SYSTRAP(__clone)
	_INVOKE_CERROR()

	/*
	 * x1 and x0 are the same as from fork:
	 * x1 == 0 in parent process, x1 == 1 in child process.
	 * x0 == pid of child in parent, x0 == pid of parent in child.
	 */
	/* if this is the parent then just return the pid */
	cbnz	x1, 1f

	/*
	 * This is the child
	 * load the function and arg off the stack
	 */
	ldp	x0, x1, [sp], #16

	/* call the function */
	blr	x1

	/* call _exit with the returned value */
	bl	_C_LABEL(_exit)
1:	ret
.Leinval:
	mov	x0, #EINVAL
	b	_C_LABEL(__cerror)
END(__clone)
