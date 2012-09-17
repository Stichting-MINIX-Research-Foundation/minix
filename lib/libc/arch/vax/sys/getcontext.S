/*	$NetBSD: getcontext.S,v 1.6 2011/11/21 16:17:48 chs Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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

#include "SYS.h"

#ifdef SYSLIBC_SCCS
RCSID("$NetBSD: getcontext.S,v 1.6 2011/11/21 16:17:48 chs Exp $")
#endif

#ifdef WEAK_ALIAS
WEAK_ALIAS(getcontext, _getcontext)
#endif

/*
 * Can't use R4 or R5 since those needed by the phread_switch code.
 */
ENTRY(_getcontext, 0)
	clrl	%r0			/* assume success */
	chmk	$SYS_getcontext		/* getcontext(oucp) */
	jcc	1f
	jmp	CERROR+2		/* badness happened */

1:	movl	4(%ap),%r0		/* ptr to ucontext */
	movl	16(%fp),%r1		/* PC to return to */
	movab	2f,16(%fp)		/* let's return early */
	ret
	/*
	 * Now we have the blown away the callframe and argument
	 * list so it's as if we've return from the function except
	 * we aren't executing the caller's code (yet).  So it's
	 * the callers state but we still have control.
	 */
	_ALIGN_TEXT
2:	movq	%ap,(36+12*4)(%r0)	/* adjust AP + SP */
	movl	%fp,(36+14*4)(%r0)	/* adjust FP */
	movl	%r3,(36+15*4)(%r0)	/* adjust PC */
	clrl	%r0
	jmp	(%r1)			/* and return */
END(_getcontext)
