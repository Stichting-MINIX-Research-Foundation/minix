/*	$NetBSD: swapcontext.S,v 1.5 2012/09/12 02:00:52 manu Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
#include <machine/mcontext.h>

#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: swapcontext.S,v 1.5 2012/09/12 02:00:52 manu Exp $")
#endif /* SYSLIBC_SCCS && !lint */

LEAF_ENTRY(swapcontext)
	stw	%rp, HPPA_FRAME_CRP(%sp)
	stw	%arg0, HPPA_FRAME_ARG(0)(%sp)
	stw	%arg1, HPPA_FRAME_ARG(1)(%sp)
	SYSCALL(getcontext)

	ldw	HPPA_FRAME_ARG(0)(%sp), %arg1
	ldw	HPPA_FRAME_ARG(1)(%sp), %arg0

	ldw	HPPA_FRAME_CRP(%sp), %rp
	ldo	4(%rp), %r1
	stw	%r1, (_OFFSETOF_UC_GREGS + _REG_PCOQT * SZREG)(%arg1)
	stw	%rp, (_OFFSETOF_UC_GREGS + _REG_PCOQH * SZREG)(%arg1)
	bl	setcontext, %rp
	stw	%r0, (_OFFSETOF_UC_GREGS + _REG_RET0 * SZREG)(%arg1)

	ldw	HPPA_FRAME_CRP(%sp), %rp
	bv,n	%r0(%rp)
EXIT(swapcontext)
	.end
