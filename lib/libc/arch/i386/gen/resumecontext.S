/*	$NetBSD: resumecontext.S,v 1.8 2014/05/23 02:34:19 uebayasi Exp $	*/

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
#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: resumecontext.S,v 1.8 2014/05/23 02:34:19 uebayasi Exp $")
#endif /* LIBC_SCCS && !lint */

/*
 * This assembly-language implementation differs from the (obvious)
 * C-language implementation only in not clobbering the previous
 * function's return address (us), which is the point of the exercise.
 */

NENTRY(_resumecontext)	/* profiling prologue would clobber TOS */
	leal	-(4 + 776)(%esp),%ecx	/* retaddr + sizeof (ucontext_t) */
	andl	$~(0xf),%ecx		/* align on _UC_UCONTEXT_ALIGN */
	movl	%ecx,%esp
	PIC_PROLOGUE
	pushl	%ecx
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(_getcontext))
#else
	call	_C_LABEL(_getcontext)
#endif
	addl	$4,%esp
	PIC_EPILOGUE
	movl	4(%esp),%ecx		/* uc_link */
	PIC_PROLOGUE
	pushl	%ecx	/* argument for exit() or setcontext() */
	testl	%ecx,%ecx		/* link end? */
	jnz	9f	/* watch out for local label use in PIC_* */

	/* normal exit */
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(exit))
#else
	call	_C_LABEL(exit)
#endif
	/* NOTREACHED */

9:
#ifdef __PIC__
	call	PIC_PLT(_C_LABEL(setcontext))
#else
	call	_C_LABEL(setcontext)
#endif
	/* NOTREACHED */

	/* something is wrong, pull the brake */
	pushl	$-1
	pushl	%eax	/* unused return address */
	SYSTRAP(exit)
END(_resumecontext)
