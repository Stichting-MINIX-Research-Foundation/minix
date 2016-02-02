/*	$NetBSD: swapcontext.S,v 1.8 2014/05/23 02:34:19 uebayasi Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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

#include <machine/asm.h>
#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: swapcontext.S,v 1.8 2014/05/23 02:34:19 uebayasi Exp $")
#endif /* LIBC_SCCS && !lint */

ENTRY(swapcontext)
	movl	4(%esp),%ecx
	PIC_PROLOGUE
	pushl	%ecx			/* push oucp */
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(_getcontext))
#else
	call	_C_LABEL(_getcontext)
#endif
	addl	$4,%esp			/* pop oucp */
	PIC_EPILOGUE
	testl	%eax,%eax
	jnz	2f
	movl	4(%esp),%ecx
	movl	0(%esp),%edx
	movl	%edx,(36 + 14 * 4)(%ecx)	/*  Adjust saved %eip ... */
#ifdef __PIC__
	movl	%ebx,(36 +  8 * 4)(%ecx)	/* ... clobbered PIC reg ... */
#endif
	leal	4(%esp),%edx
	movl	%edx,(36 + 17 * 4)(%ecx)	/* ... and %esp. */
	/* Note: oucp->%eax has been cleared by getcontext(). */
	movl	8(%esp),%ecx
	PIC_PROLOGUE
	pushl	%ecx			/* push ucp */
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(setcontext))
#else
	call	_C_LABEL(setcontext)
#endif
	addl	$4,%esp			/* pop ucp */
	PIC_EPILOGUE
	/* No need to error-check: setcontext() must have failed. */
2:	ret	
END(swapcontext)
