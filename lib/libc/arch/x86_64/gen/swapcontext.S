/*	$NetBSD: swapcontext.S,v 1.6 2014/05/22 15:01:56 uebayasi Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: swapcontext.S,v 1.6 2014/05/22 15:01:56 uebayasi Exp $")
#endif /* LIBC_SCCS && !lint */

/*
 * 56 == offsetof(ucontext_t, uc_mcontext)
 * 21 == _REG_RIP
 * 24 == _REG_URSP
 */

ENTRY(swapcontext)
	pushq	%rdi				/* preserve oucp */
	pushq	%rsi				/* preserve ucp */
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(_getcontext))
#else
	call	_C_LABEL(_getcontext)
#endif
	popq	%rsi
	popq	%rdi
	testl	%eax,%eax
	jnz	2f
	movq	0(%rsp),%r11
	movq	%r11,(56 + 21 * 8)(%rdi)
	leaq	8(%rsp),%r11
	movq	%r11,(56 + 24 * 8)(%rdi)
	movq	%rsi,%rdi
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(setcontext))
#else
	call	_C_LABEL(setcontext)
#endif
2:
	ret
END(swapcontext)
