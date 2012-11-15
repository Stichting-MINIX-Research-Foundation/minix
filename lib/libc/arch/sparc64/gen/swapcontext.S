/*	$NetBSD: swapcontext.S,v 1.7 2012/09/13 11:47:46 martin Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: swapcontext.S,v 1.7 2012/09/13 11:47:46 martin Exp $")
#endif /* LIBC_SCCS && !lint */


ENTRY(swapcontext)
	mov	%o0, %o2			/* must save pointer */
	mov	%o1, %o3			/* must save pointer */
	mov	SYS_getcontext, %g1
	t	ST_SYSCALL
	bcc,a	1f
	 stx	%g0, [%o2 + 64 + 11 * 8]	/* gr[_REG_O0] = 0 */
	ERROR()
1:
	add	%o7, 12, %o1
	stx	%o1, [%o2 + 64 + 2 * 8]		/* gr[_REG_nPC] = retaddr + 4 */
	add	%o7, 8, %o1
	stx	%o1, [%o2 + 64 + 1 * 8]		/* gr[_REG_PC] = retaddr */

#ifdef	PIC
	PIC_PROLOGUE(%g5,%o4)			/* %g5 = _GLOBAL_OFFSET_TABLE */
#ifdef	BIGPIC
	set	_C_LABEL(setcontext), %o5
	ldx	[%g5+%o5], %o4
#else
	ldx	[%g5+_C_LABEL(setcontext)], %o4
#endif
	jmp	%o4
	 mov	%o3, %o0
#else
	ba,a	_C_LABEL(setcontext)
	 mov	%o3, %o0
#endif
