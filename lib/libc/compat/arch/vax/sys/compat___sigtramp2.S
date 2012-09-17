/*	$NetBSD: compat___sigtramp2.S,v 1.3 2011/01/29 22:58:47 matt Exp $	*/

/*
 * Copyright (c) 2002 Anders Magnusson, (ragge@NetBSD.org).
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
 *	sp, pc, psl - obvious
 *	ap - points to argument list, see machdep.c:sendsig()
 *	fp - address of signal handler
 */

#include "SYS.h"

#ifdef SYSLIBC_SCCS
RCSID("$NetBSD: compat___sigtramp2.S,v 1.3 2011/01/29 22:58:47 matt Exp $")
#endif

	.text
	_ALIGN_TEXT

	.globl	_C_LABEL(__sigtramp_sigcontext_2)
_C_LABEL(__sigtramp_sigcontext_2):
	nop; nop;
	pushr $(R0|R1|R2|R3|R4|R5)	# save scratch registers
	callg (%ap),(%fp)		# use global arg list
	popr $(R0|R1|R2|R3|R4|R5)	# restore regs
	movab 8(%ap),%ap		# arg is pointer to sigctx
	SYSTRAP(compat_16___sigreturn14) # exit from here
	halt				# illegal insn
END(__sigtramp_sigcontext_2)
