/*	$NetBSD: __vfork14.S,v 1.6 2014/03/06 19:02:58 skrll Exp $	*/

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

ENTRY(__vfork14, 0)
	/*
	 * NB: __vfork14 is a tricky syscall.  We can't save
	 * any values on the stack, because the stack will be 
	 * trashed by the child, leaving garbage for when the
	 * parent returns.
	 *
	 * So we must save values in registers, specifically,
	 * registers that the kernel is willing to preserve
	 * across the syscall.  Normally, this would be callee-
	 * saved registers, with the kernel being the callee,
	 * but the problem is that we, too, are a callee, and
	 * would have to save that very same register somewhere,
	 * because *our* caller is counting on us to do so.
	 *
	 * What we need is a normally caller-saved register,
	 * that the kernel is willing to save for us.  The 
	 * syscall entry code in locore.S has been modified 
	 * to do just this for the t4 register.
	 */
	.import	__cerror, code

	copy	%rp, %t4
	ldil	L%SYSCALLGATE, %r1
	ble	4(%sr2, %r1)
	ldi	SYS___vfork14, %t1
	comb,<>	%r0, %t1, __cerror
	copy	%t4, %rp
	addi	-1, %ret1, %ret1
	bv	%r0(%rp)
	and	%ret0, %ret1, %ret0
EXIT(__vfork14)
