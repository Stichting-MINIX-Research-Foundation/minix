/*	$NetBSD: _resumecontext.S,v 1.6 2009/12/14 01:07:42 matt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wayne Knowles.
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

#include "SYS.h"
#include "assym.h"

#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: _resumecontext.S,v 1.6 2009/12/14 01:07:42 matt Exp $")
#endif /* SYSLIBC_SCCS && !lint */

	.set	reorder
	.hidden	_C_LABEL(__resumecontext)
	
LEAF_NOPROFILE(__resumecontext)
	/*
	 * We get here not by a call through $t9 but thru $ra after the
	 * function passed to makecontext returns.
	 */
	PTR_SUBU	sp, sp, UCONTEXT_SIZE		# get space for ucontext
	move		a0, sp				# arg0 for getcontext
	PTR_S		zero, _OFFSETOF_UC_LINK(a0)	# make sure uc_link is 0
	SYSTRAP(getcontext)				# get context
	PTR_L		a0, _OFFSETOF_UC_LINK(a0)	# linked context?
	beq		a0, zero, 1f			#   nope, exit process
	nop
	SYSTRAP(setcontext)				#   yes, become it.
	/* NOTREACHED (in theory) */
	li		a0, -1				# failure, 
1:
	SYSTRAP(exit)					# all hope is lost.
	/* NOTREACHED */
END(__resumecontext)
