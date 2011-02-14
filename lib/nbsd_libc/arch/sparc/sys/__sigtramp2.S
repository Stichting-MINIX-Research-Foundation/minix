/*	$NetBSD: __sigtramp2.S,v 1.2 2008/04/28 20:22:57 martin Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: __sigtramp2.S,v 1.2 2008/04/28 20:22:57 martin Exp $")
#endif /* SYSLIBC_SCCS and not lint */

#include "SYS.h"

/*
 * When this code is run, the stack looks like:
 *	[%sp]			struct frame
 *	[%sp + 96]		siginfo_t
 *	[%sp + 96 + 128]	ucontext_t
 *	    .
 *	    .
 */
ENTRY_NOPROFILE(__sigtramp_siginfo_2)
	add	%sp, 96 + 128, %o0	/* get pointer to ucontext */
	mov	SYS_setcontext, %g1
	t	ST_SYSCALL		/* call setcontext */
	mov	SYS_exit, %g1		/* exit with errno */
	t	ST_SYSCALL		/* if sigreturn fails */
