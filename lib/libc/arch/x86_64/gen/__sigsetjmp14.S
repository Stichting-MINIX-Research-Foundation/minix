/*	$NetBSD: __sigsetjmp14.S,v 1.3 2014/05/22 15:01:56 uebayasi Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
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
#include <machine/setjmp.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: __sigsetjmp14.S,v 1.3 2014/05/22 15:01:56 uebayasi Exp $")
#endif

/*
 * C library -- _setjmp, _longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(__sigsetjmp14)
	movq	(%rsp),%r11
	movq	%rbx,(_JB_RBX * 8)(%rdi)
	movq	%rbp,(_JB_RBP * 8)(%rdi)
	movq	%r12,(_JB_R12 * 8)(%rdi)
	movq	%r13,(_JB_R13 * 8)(%rdi)
	movq	%r14,(_JB_R14 * 8)(%rdi)
	movq	%r15,(_JB_R15 * 8)(%rdi)
	movq	%rsp,(_JB_RSP * 8)(%rdi)
	movq	%r11,(_JB_PC  * 8)(%rdi)

	movq	%rsi,(_JB_SIGFLAG  * 8)(%rdi)
	testl	%esi,%esi
	jz	2f

	leaq	(_JB_SIGMASK * 8)(%rdi),%rdx
	xorl	%edi,%edi
	xorq	%rsi,%rsi

#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(__sigprocmask14))
#else
	call	_C_LABEL(__sigprocmask14)
#endif
2:	xorl	%eax,%eax
	ret
END(__sigsetjmp14)

ENTRY(__siglongjmp14)
	movq	%rdi,%r12
	pushq	%rsi
	cmpl	$0, (_JB_SIGFLAG * 8)(%rdi)

	jz	2f
	leaq	(_JB_SIGMASK * 8)(%rdi),%rsi
	movl	$3,%edi		/* SIG_SETMASK */
	xorq	%rdx,%rdx

#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(__sigprocmask14))
#else
	call	_C_LABEL(__sigprocmask14)
#endif
2:	popq	%rax
	movq	(_JB_RBX * 8)(%r12),%rbx
	movq	(_JB_RBP * 8)(%r12),%rbp
	movq	(_JB_R13 * 8)(%r12),%r13
	movq	(_JB_R14 * 8)(%r12),%r14
	movq	(_JB_R15 * 8)(%r12),%r15
	movq	(_JB_RSP * 8)(%r12),%rsp
	movq	(_JB_PC  * 8)(%r12),%r11
	movq	(_JB_R12 * 8)(%r12),%r12

	testl	%eax,%eax
	jnz	1f
	incl	%eax
1:	movq	%r11,0(%rsp)
	ret
END(__siglongjmp14)
