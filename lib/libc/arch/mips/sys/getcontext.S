/*	$NetBSD: getcontext.S,v 1.4 2009/12/14 01:07:42 matt Exp $	*/

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
#include "assym.h"
#include <machine/mcontext.h>

#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: getcontext.S,v 1.4 2009/12/14 01:07:42 matt Exp $")
#endif /* SYSLIBC_SCCS && !lint */

#ifdef WEAK_ALIAS
WEAK_ALIAS(getcontext, _getcontext)
#endif
	.set reorder

LEAF(_getcontext)
	PIC_PROLOGUE(_getcontext)
	SYSTRAP(getcontext)
	bne		a3, zero, 1f
	REG_PROLOGUE
	REG_S		zero, (_OFFSETOF_UC_GREGS + _REG_V0 * SZREG)(a0)
	REG_S		ra, (_OFFSETOF_UC_GREGS + _REG_EPC * SZREG)(a0)
	REG_EPILOGUE
	PIC_RETURN()
1:
	PIC_TAILCALL(__cerror)
END(_getcontext)
