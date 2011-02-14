/*	$NetBSD: __sigtramp2.S,v 1.4 2009/11/03 05:07:25 snj Exp $	*/

/*
 * Copyright (c) 1998-2001 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portitions of this file are derived from other sources, see
 * the copyrights and acknowledgements below.
 */
/*
 * Copyright (c) 1990,1991,1992,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * THE UNIVERSITY OF UTAH AND CSL PROVIDE THIS SOFTWARE IN ITS "AS IS"
 * CONDITION, AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM ITS USE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: locore.s 1.62 94/12/15$
 */
/*
 *  (c) Copyright 1988 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */

#include "SYS.h"

/*
 * The hppa signal trampoline is required to call the handler
 * possibily via a PLABEL.
 *
 * On entry, stack looks like:
 *
 *	sp->					[x]
 * r3,arg2->	ucontext structure
 *    arg1->	siginfo structure		[0]
 *
 * where
 *	x = HPPA_FRAME_ROUND(sizeof(sigframe_siginfo))
 *
 * and
 * 	struct sigframe_siginfo {
 *		siginfo_t sf_si;
 *		ucontext_t sf_uc;
 *	};
 */

ENTRY_NOPROFILE(__sigtramp_siginfo_2, 0)
	.call

	/*
	 * Our sendsig() places the address of the signal handler in %arg3.
	 * It may actually be a PLABEL.
	 */
	bb,>=,n	%arg3, 30, L$sigcode_bounce	; branch if not a PLABEL
	depi	0, 31, 2, %arg3			; zero L bit in PLABEL pointer
	ldw	4(%arg3), %r19			; load shared library linkage
	ldw	0(%arg3), %arg3			; load real catcher address

L$sigcode_bounce:

	/*
	 * This blr puts the address of the following nop in rp.
	 * It also schedules the nop for execution, which is why
	 * that instruction has to be a nop, or, rather, not any
	 * instruction only meant to execute once the signal handler
	 * returns.
	 */
	blr	%r0, %rp

	/*
	 * This bv schedules the instruction pointed to by arg3
	 * for execution.  So, arg3 is the address of the signal
	 * handler.
	 */
	bv,n	%r0(%arg3)
	nop

	/*
	 * The signal handler has returned.  Since r3 is on the list
	 * of callee-saved registers, it's whatever the sendsig
	 * code wanted it set to.  Since we copy it into arg0,
	 * it looks like sendsig leaves r3 holding the desired
	 * single argument to sys_setcontext, i.e., the ucontext_t *.
	 */

	/* Make a SYS_setcontext system call. */
	copy	%r3, %arg0
	ldil	L%SYSCALLGATE, %r1
	.call
	ble	4(%sr7, %r1)
	ldi	SYS_setcontext, %t1

	/* Make a SYS_exit system call. */
	copy	%ret0, %arg0
	ldil	L%SYSCALLGATE, %r1
	.call
	ble	4(%sr7, %r1)
	ldi	SYS_exit, %t1
EXIT(__sigtramp_siginfo_2)
