/*	$NetBSD: swapcontext.S,v 1.4 2011/01/25 02:38:15 matt Exp $	*/

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

#ifdef LIBC_SCCS
RCSID("$NetBSD: swapcontext.S,v 1.4 2011/01/25 02:38:15 matt Exp $")
#endif

ENTRY(swapcontext, 0)
	clrl	%r0			/* assume getcontext succeeds */
	chmk	$SYS_getcontext		/* getcontext(oucp) */
	jcc	1f
	jmp	CERROR+2		/* something bad happened */

1:	movl	4(%ap),%r0		/* get oucp */
	addl3	(%ap),$(5+1),%r1	/* size of callframe + arglist (LW) */
	bbs	$13,6(%fp),2f		/* was this a CALLG? */
	movl	$5,%r1			/* size of callframe (in LW) */
2:	movl	8(%fp),(36+12*4)(%r0)		/* restore AP */
	moval	(%sp)[%r1],(36+13*4)(%r0)	/* restore SP */
	movq	12(%fp),(36+14*4)(%r0)		/* restore FP + PC */

	pushl	8(%ap)
	calls	$1,_C_LABEL(setcontext) /* setcontext(ucp) */
	ret
END(swapcontext)
