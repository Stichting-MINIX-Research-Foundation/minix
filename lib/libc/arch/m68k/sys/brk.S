/*	$NetBSD: brk.S,v 1.19 2013/09/12 15:36:15 joerg Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)brk.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: brk.S,v 1.19 2013/09/12 15:36:15 joerg Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

	.globl	_end
	.globl	_C_LABEL(__minbrk)
	.hidden	_C_LABEL(__minbrk)
	.globl	_C_LABEL(__curbrk)
	.hidden	_C_LABEL(__curbrk)

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.data
_C_LABEL(__minbrk):
	.long	_end

	.text

ENTRY(_brk)
#ifdef __PIC__
	LEA_LCL(_C_LABEL(__minbrk),%a1)
	movl	(%a1),%a1
#else
	movl	_C_LABEL(__minbrk),%a1
#endif
	cmpl	4(%sp),%a1
	jls	ok
	movl	%a1,4(%sp)
ok:
	SYSTRAP(break)
	jcs	CERROR
#ifdef __PIC__
	LEA_LCL(_C_LABEL(__curbrk),%a0)
	movl	%a1,(%a0)
#else
	movl	%a1,_C_LABEL(__curbrk)
#endif
	clrl	%d0
	rts
END(_brk)
