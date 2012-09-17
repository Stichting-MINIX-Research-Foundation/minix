/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and a struct sigcontext, see <signal.h>
 */

#include "DEFS.h"
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
	/* .asciz "@(#)setjmp.s	8.1 (Berkeley) 6/4/93" */
RCSID("$NetBSD: __setjmp14.S,v 1.11 2011/01/25 02:38:15 matt Exp $")
#endif /* LIBC_SCCS and not lint */

ENTRY(__setjmp14, R6)
	movl	4(%ap),%r2		# construct sigcontext
	subl2	$12,%sp			# space for current struct sigstack
	pushl	%sp			# get current values
	pushl	$0			# no new values
	calls	$4,_C_LABEL(__sigaltstack14) # pop args plus signal stack value
	movl	4(%ap),%r2		# construct sigcontext
	movl	(%sp)+,(%r2)		# save onsigstack status of caller
	pushal	28(%r2)
	pushl	$0
	pushl	$0
	calls	$3,_C_LABEL(__sigprocmask14) # get signal mask
	addl3	$8,4(%ap),%r2		# point to sp in signal context
	clrl	%r0			# assume no stack arguments
	bbc	$13,6(%fp),1f		# handle callg
	addl3	$1,(%ap),%r0		# get argument count +1 if calls
1:	moval	24(%fp)[%r0],(%r2)+	# save sp of caller
	movl	12(%fp),(%r2)+		# save frame pointer of caller
	movl	8(%fp),(%r2)+		# save argument pointer of caller
	movl	16(%fp),(%r2)+		# save pc of caller
	movpsl	(%r2)			# save current psl
	movw	4(%fp),(%r2)		# save psw of caller
	addl3	$SC_LEN,4(%ap),%r2	# point to past signal context
	movq	%r6,(%r2)+		# save r6/r7
	movq	%r8,(%r2)+		# save r8/r9
	movq	%r10,(%r2)+		# save r10/r11
	clrl	%r0
	ret
END(__setjmp14)
