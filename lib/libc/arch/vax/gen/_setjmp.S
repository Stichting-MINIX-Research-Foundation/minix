/*
 * Copyright (c) 1980, 1993
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

#include "DEFS.h"

#if defined(LIBC_SCCS) && !defined(lint)
	/* .asciz "@(#)_setjmp.s	8.1 (Berkeley) 6/4/93" */
RCSID("$NetBSD: _setjmp.S,v 1.10 2011/01/25 02:38:15 matt Exp $")
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	_setjmp(a)
 * by restoring registers from the stack,
 * The previous signal state is NOT restored.
 *
 * Even though we don't use sigreturn14, we still store things in a sigcontext
 * in order to be consistent.
 */

ENTRY(_setjmp, R6)
	movl	4(%ap),%r0
	movl	12(%fp),12(%r0)		# save frame pointer of caller
	movl	16(%fp),20(%r0)		# save pc of caller
	movl	8(%fp),16(%r0)		# save ap of caller
	clrl	%r1			# clear arg count
	bbc	$13,6(%fp),1f		# was this a callg?
	addl3	$1,(%ap),%r1		# get real arg count+1 for calls
1:	moval	24(%fp)[%r1],8(%r0)	# save sp of caller
	movpsl	24(%r0)			# save current psl
	movw	4(%fp),24(%r0)		# save psw of caller
	movq	%r6,44(%r0)		# save r6/r7
	movq	%r8,52(%r0)		# save r8/r9
	movq	%r10,60(%r0)		# save r10/r11
	clrl	%r0
	ret
END(_setjmp)

ENTRY(_longjmp, 0)
	movl	8(%ap),%r0		# return(v)
	movl	4(%ap),%r1		# fetch buffer
	tstl	12(%r1)			# is fp null
	beql	botch
	movq	44(%r1),%r6		# restore r6/r7
	movq	52(%r1),%r8		# restore r8/r9
	movq	60(%r1),%r10		# restore r10/r11
	movl	16(%r1),%ap		# restore ap
	movl	8(%r1),%sp		# restore sp
	movl	12(%r1),%fp		# restore fp
	movq	20(%r1),-(%sp)		# save pc/psl to new stack
	rei				# and go back to saved pc/psl

botch:
	calls	$0,_C_LABEL(longjmperror)
	halt
END(_longjmp)
