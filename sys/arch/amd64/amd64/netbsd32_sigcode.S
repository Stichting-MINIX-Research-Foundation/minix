/*	$NetBSD: netbsd32_sigcode.S,v 1.9 2018/07/14 14:29:40 maxv Exp $	*/

/*
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include "assym.h"
#include <machine/asm.h>
#include <sys/syscall.h>

/*
 * Signal trampoline for 1.6 compatibility; copied to top of user stack.
 */
	.code32
ENTRY(netbsd32_sigcode)
	/*
	 * Handler has returned here as if we called it.  The sigcontext
	 * is on the stack after the 3 args "we" pushed.
	 */
	leal	12(%esp),%eax		/* get pointer to sigcontext */
	movl	%eax,4(%esp)		/* put it in the argument slot */
					/* fake return address already there */
	movl	$SYS_compat_16___sigreturn14,%eax
	int	$0x80			/* enter kernel with args on stack */
	movl	$SYS_exit,%eax
	int	$0x80			/* exit if sigreturn fails */
	.globl	_C_LABEL(netbsd32_esigcode)
_C_LABEL(netbsd32_esigcode):
END(netbsd32_sigcode)

/*
 * There is no NetBSD-1.6 compatibility for native code.
 * COMPAT_16 is useful for i386 emulation (COMPAT_NETBSD32) only.
 */
	.globl _C_LABEL(sigcode), _C_LABEL(esigcode)
	.set _C_LABEL(sigcode),0
	.set _C_LABEL(esigcode),0
