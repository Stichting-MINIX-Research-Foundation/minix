/* $NetBSD: __syscall.S,v 1.1 2014/08/10 05:47:37 matt Exp $ */

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

#ifndef FUNCNAME
#define	FUNCNAME	__syscall
#define SYSTRAP_SYSCALL	SYSTRAP(__syscall)
#endif

#if SYS_MAXSYSARGS > 8
#error SYS_MAXSYSARGS is greater than 8
#endif

ENTRY(FUNCNAME)
	/*
	 * AAPCS64 supplies the first 8 arguments in registers but since
	 * the first argument is the syscall #, we only have room for 7
	 * arguments.  Thus if the syscall uses all 8 arguments, the last
	 * argument will have been placed on the stack.  Rather than
	 * complicating the kernel syscall code to deal with that, we fixup
	 * things here.
	 *
	 * First we move the syscall number to x17 freeing a register.
	 * Then we move the rest of the arguments to their proper register.
	 * Finally, load x7 from the stack.
	 *
	 * Now everything looks as if the syscall wrapper was directly called.
	 * (unless it was for pipe, fork, vfork, ptrace, or __clone).
	 */
	mov	x17, x0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	mov	x3, x4
	mov	x4, x5
	mov	x5, x6
	mov	x6, x7
	ldr	x7, [sp, #0]
	SYSTRAP_SYSCALL
	_INVOKE_CERROR()
	ret
END(FUNCNAME)
