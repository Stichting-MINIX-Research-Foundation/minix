/*	$NetBSD: __clone.S,v 1.3 2001/07/18 12:24:53 fvdl Exp $	*/

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
	pushl	%ebp

	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	movl	8(%esp), %ebp
	cmpl	$0,%ebp		/* function */
	je	3f
	movl	12(%esp),%eax	/* stack */
	cmpl	$0,%eax
	je	3f

	/*
	 * Set up the stack for the clone.
	 */
	movl	20(%esp),%ecx
	movl	%ecx,-4(%eax)	/* argument */
	leal	-4(%eax),%eax	/* sp points to arg */

	pushl	%eax		/* stack */
	pushl	20(%esp)	/* flags */
	pushl	$0		/* dummy return address */

	SYSTRAP(__clone)
	jc	4f
	cmpl	$0,%eax
	jne	2f		/* we're the parent */
	call	*%ebp		/* this is the clone, call the function */

#ifdef PIC
	PIC_PROLOGUE
	pushl	%eax		/* clone does _exit(func(arg)); */
	call	PIC_PLT(_C_LABEL(_exit))
	addl	$4,%esp
	PIC_EPILOGUE
#else
	pushl	%eax
	call	_C_LABEL(_exit)
	addl	$4,%esp
#endif

2:
	addl	$12,%esp
	popl	%ebp
	ret
3:
	movl	$EINVAL,%eax
	jmp	5f
4:
	addl	$12,%esp
5:
	popl	%ebp
#ifdef PIC
	PIC_PROLOGUE
	movl	PIC_GOT(CERROR),%edx
	PIC_EPILOGUE
	jmp	*%edx
#else
	jmp	CERROR
#endif
