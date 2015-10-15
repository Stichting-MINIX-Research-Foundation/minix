/*	$NetBSD: ptrace.S,v 1.6 2014/11/24 15:33:18 christos Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)ptrace.s	5.1 (Berkeley) 4/23/90
 */

#include <machine/asm.h>
#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: ptrace.S,v 1.6 2014/11/24 15:33:18 christos Exp $")
#endif /* SYSLIBC_SCCS and not lint */

#include "SYS.h"

	.globl	_C_LABEL(__errno)

ENTRY(ptrace)
	/*
	 * The following code calls __errno() to set it to 0 before
	 * calling ptrace(2). The libc version of __errno() does not use
	 * any registers, but the libpthread version clobbers %rcx
	 * before we get a chance to store it in %r10. So we save it
	 * in %r10 and restore it.
	 */
	movq	%rcx, %r10
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(__errno))
#else
	call	_C_LABEL(__errno)
#endif /* __PIC__ */
	movl	$0,(%rax)
	movq	%r10, %rcx
	SYSTRAP(ptrace)
	jc	err
	ret
err:
	jmp	CERROR
END(ptrace)
