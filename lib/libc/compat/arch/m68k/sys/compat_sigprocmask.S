/*	$NetBSD: compat_sigprocmask.S,v 1.4 2013/07/25 22:08:15 matt Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)sigprocmask.s	5.2 (Berkeley) 6/6/90")
#else
	RCSID("$NetBSD: compat_sigprocmask.S,v 1.4 2013/07/25 22:08:15 matt Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

WARN_REFERENCES(sigprocmask, \
    "warning: reference to compatibility sigprocmask(); include <signal.h> for correct reference")

ENTRY(sigprocmask)
	tstl	8(%sp)			/* check new sigset pointer */
	jne	gotptr			/* if not null, indirect */
/*	movl	#0,8(%sp)		/* null mask pointer: block empty set */
	movql	#1,%d0			/* SIG_BLOCK */
	movl	%d0,4(%sp)
	jra	doit
gotptr:
	movl	8(%sp),%a0
	movl	(%a0),8(%sp)		/* indirect to new mask arg */
doit:
	SYSTRAP(compat_13_sigprocmask13)
	jcs	CERROR
	tstl	12(%sp)		/* test if old mask requested */
	jeq	out
	movl	12(%sp),%a0
	movl	%d0,(%a0)		/* store old mask */
out:
	clrl	%d0
	rts
END(sigprocmask)
