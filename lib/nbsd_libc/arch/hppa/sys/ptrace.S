/*	$NetBSD: ptrace.S,v 1.6 2008/04/28 20:22:56 martin Exp $	*/

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

	.import __cerror, code

/*
 * int ptrace(int request, pid_t pid, void * addr, int data);
 */

ENTRY(ptrace, HPPA_FRAME_SIZE)
	stw	%rp, HPPA_FRAME_CRP(%sp)
	stw	%arg0, HPPA_FRAME_ARG(0)(%sp)
	stw	%arg1, HPPA_FRAME_ARG(1)(%sp)
	stw	%arg2, HPPA_FRAME_ARG(2)(%sp)
	stw	%arg3, HPPA_FRAME_ARG(3)(%sp)
	ldo	HPPA_FRAME_SIZE(%sp),%sp
	bl	__cerror, %rp
	 copy	%r0, %t1
	ldo	-HPPA_FRAME_SIZE(%sp), %sp
	ldw	HPPA_FRAME_ARG(0)(%sp), %arg0
	ldw	HPPA_FRAME_ARG(1)(%sp), %arg1
	ldw	HPPA_FRAME_ARG(2)(%sp), %arg2
	ldw	HPPA_FRAME_ARG(3)(%sp), %arg3
	ldw	HPPA_FRAME_CRP(%sp), %rp

	SYSCALL(ptrace)
	bv,n	%r0(%rp)
EXIT(ptrace)
