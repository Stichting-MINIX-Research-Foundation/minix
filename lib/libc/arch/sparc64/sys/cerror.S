/*	$NetBSD: cerror.S,v 1.8 2013/09/14 05:38:00 nakayama Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *
 * from: Header: cerror.s,v 1.3 92/07/02 04:17:59 torek Exp
 */

#include <machine/asm.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	.asciz "@(#)cerror.s	8.1 (Berkeley) 6/4/93"
#else
	RCSID("$NetBSD: cerror.S,v 1.8 2013/09/14 05:38:00 nakayama Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

#include "SYS.h"

#ifdef _REENTRANT
FUNC(__cerror)
	save	%sp, -CC64FSZ, %sp
	call	_C_LABEL(__errno)
	 nop
	st	%i0, [%o0]
	mov	-1, %i0
	ret
	 restore %g0, -1, %o1
#else
	.globl	_C_LABEL(errno)

#if __PIC__ - 0 >= 2

FUNC(__cerror)
	PIC_PROLOGUE(%g1, %o3)
	set	_C_LABEL(errno),%o1
	ldx	[%g1 + %o1], %g1
	st	%o0, [%g1]
	mov	-1, %o0
	retl
	 mov	-1, %o1

#elif __PIC__ - 0 >= 1

FUNC(__cerror)
	PIC_PROLOGUE(%g1, %o3)
	ldx	[%g1 + _C_LABEL(errno)], %g1
	st	%o0, [%g1]
	mov	-1, %o0
	retl
	 mov	-1, %o1

#else

FUNC(__cerror)
	sethi	%hi(_C_LABEL(errno)), %g1
	st	%o0, [%g1 + %lo(_C_LABEL(errno))]
	mov	-1, %o0
	retl
	 mov	-1, %o1

#endif /* __PIC__ */

#endif /* _REENTRANT */
