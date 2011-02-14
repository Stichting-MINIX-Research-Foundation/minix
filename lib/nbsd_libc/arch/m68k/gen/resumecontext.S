/*	$NetBSD: resumecontext.S,v 1.5 2008/04/28 20:22:56 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
	RCSID("$NetBSD: resumecontext.S,v 1.5 2008/04/28 20:22:56 martin Exp $")
#endif /* LIBC_SCCS && !lint */

/*
 * This assembly-language implementation differs from the (obvious)
 * C-language implementation only in not clobbering the previous
 * function's return address (us), which is the point of the exercise.
 */

ENTRY(_resumecontext)
	subl	#(4 + 1024),%sp		/* retaddr preservation + ucontext_t */
	lea	%sp@,%a0
	movl	%a0,%sp@-
	jbsr	PIC_PLT(_C_LABEL(_getcontext))
	movl	%sp@(4 + 4),%sp@	/* uc_link */
	tstl	%sp@			/* check for NULL */
	jne	1f
	jbsr	PIC_PLT(_C_LABEL(exit))	/* cleanly exit(0) */
	jmp	2f
1:	jbsr	PIC_PLT(_C_LABEL(setcontext))
	/* NOTREACHED */
	/* But just in case... */
2:	movl	#-1,%sp@
	jbsr	PIC_PLT(_C_LABEL(_exit))
	/* NOTREACHED */
