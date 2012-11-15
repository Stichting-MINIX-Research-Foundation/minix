/*	$NetBSD: __clone.S,v 1.3 2011/11/18 20:43:01 joerg Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
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
#include <sys/errno.h>
#include "SYS.h"

#ifdef WEAK_ALIAS
WEAK_ALIAS(clone, __clone)
#endif
	.text

/*
 * int clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)
	pushq	%r12
	pushq	%r13
	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	testq	%rdi,%rdi
	je	3f
	testq	%rsi,%rsi
	je	3f


	movq	%rdi,%r12
	movq	%rcx,%r13

	movq	%rdx,%rdi

	pushq	$0		/* dummy return address */

	SYSTRAP(__clone)
	jc	4f
	cmpl	$0,%eax
	jne	2f		/* we're the parent */
	movq	%r13,%rdi	/* restore argument */
	call	*%r12		/* this is the clone, call the function */

	movq	%rax,%rdi
#ifdef PIC
	call	PIC_PLT(_C_LABEL(_exit))
#else
	call	_C_LABEL(_exit)
#endif

2:
	addq	$8,%rsp
	popq	%r13
	popq	%r12
	ret
3:
	movl	$EINVAL,%eax
	jmp	5f
4:
	addq	$8,%rsp
5:
	popq	%r13
	popq	%r12
	jmp	CERROR
