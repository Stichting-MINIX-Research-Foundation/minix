/*	$NetBSD: getcontext.S,v 1.3 2008/04/28 20:22:57 martin Exp $	*/

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

#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: getcontext.S,v 1.3 2008/04/28 20:22:57 martin Exp $")
#endif /* SYSLIBC_SCCS && !lint */

#ifdef WEAK_ALIAS
WEAK_ALIAS(getcontext, _getcontext)
#endif

ENTRY(_getcontext)
	mov	%o0, %o2			/* must save pointer */
	mov	SYS_getcontext, %g1
	t	ST_SYSCALL
	bcc,a	1f
	 stx	%g0, [%o2 + 64 + 11 * 8]	/* gr[_REG_O0] = 0 */
	ERROR()
1:
	add	%o7, 8, %o1
	stx	%o1, [%o2 + 64 + 1 * 8]		/* gr[_REG_PC] = retaddr */
	add	%o7, 12, %o1
	retl
	 stx	%o1, [%o2 + 64 + 2 * 8]		/* gr[_REG_nPC] = retaddr + 4 */
