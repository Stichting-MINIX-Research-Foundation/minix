/*	$NetBSD: __clone.S,v 1.3 2008/04/28 20:22:56 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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

#include <sys/errno.h>
#include "SYS.h"

#ifdef WEAK_ALIAS
WEAK_ALIAS(clone, __clone)
#endif

/*
 * int clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)
	movl	%sp@(4),%d0	/* NULL function pointer? */
	jeq	2f		/* Yup, bomb out */
	movl	%d0,%a1
	movl	%sp@(8),%d0	/* NULL stack? */
	jeq	2f		/* Yup, bomb out */
	movl	%d0,%a0
	movl	%sp@(16),%a0@-	/* Push clone's `arg' on its new stack */
	lea	%a0@(-12),%a0	/* Fake syscall args for the clone */
	movl	%a0,%sp@-	/* Syscall arg: stack */
	movl	%sp@(16),%sp@-	/* Syscall arg: flags */
	clrl	%sp@-		/* Fake return address */
	SYSTRAP(__clone)	/* Note: `fn' in a1@ is preserved */
	lea	%sp@(12),%sp	/* Zap syscall args */
	jcs	3f		/* Punt if syscall failed */
	tstl	%d0
	jne	1f		/* We're the parent, just return. */
	jsr	%a1@		/* We're the clone, call the function */
	movl	%d0,%sp@-	/* If clone returns, invoke _exit(3) */
	jbsr	PIC_PLT(_C_LABEL(_exit))
	/* NOTREACHED */
1:	rts
2:	movl	#EINVAL,%d0
3:	jra	PIC_PLT(CERROR)
