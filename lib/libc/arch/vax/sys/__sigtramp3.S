/*	$NetBSD: __sigtramp3.S,v 1.2 2011/01/25 02:38:15 matt Exp $	*/

/*
 * Copyright (c) 2003 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Signal trampoline; registers when called:
 *	pc, psl - obvious
 *	sp, ap - points to argument list
 *	    4(ap) -- signo
 *	    8(ap) -- pointer to siginfo
 *	   12(ap) -- pointer to ucontext
 *	fp - address of signal handler
 */

#include "SYS.h"

#ifdef SYSLIBC_SCCS
RCSID("$NetBSD: __sigtramp3.S,v 1.2 2011/01/25 02:38:15 matt Exp $")
#endif

	.text
	_ALIGN_TEXT

	.globl	_C_LABEL(__sigtramp_siginfo_3)
_C_LABEL(__sigtramp_siginfo_3):
	nop; nop
	callg (%ap),(%fp)		# use global arg list
	addl2 $8,%ap			# arg is pointer to ucontext
	SYSTRAP(setcontext)		# exit from here
	halt				# illegal insn
