/*	$NetBSD: brk.S,v 1.15 2003/12/26 11:21:48 martin Exp $	*/

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
	RCSID("$NetBSD: brk.S,v 1.15 2003/12/26 11:21:48 martin Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

	.globl	_end
	.globl	_C_LABEL(__minbrk)
	.globl	CURBRK

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.data
_C_LABEL(__minbrk):
	.long	_end

	.text

ENTRY(_brk)
#ifdef	PIC
#ifdef	__ELF__
	lea	%pc@(_GLOBAL_OFFSET_TABLE_@GOTPC),%a1
	movl	%a1@(_C_LABEL(__minbrk)@GOT:w),%a0
#else
	movl	#_C_LABEL(_GLOBAL_OFFSET_TABLE_),%a1
	lea	%pc@(0,a1:l),%a1
	movl	%a1@(_C_LABEL(__minbrk):w),%a0
#endif
	movl	%a0@,%d0
#else
	movl	_C_LABEL(__minbrk),%d0
#endif
	cmpl	%sp@(4),%d0
	jls	ok
	movl	%d0,%sp@(4)
ok:
	movl	#SYS_break,%d0
	trap	#0
	jcs	err
#ifdef	PIC
#ifdef	__ELF__
	movl	%a1@(CURBRK@GOT:w),%a0
#else
	movl	%a1@(CURBRK:w),%a0
#endif
	movl	%sp@(4),%a0@
#else
	movl	%sp@(4),CURBRK
#endif
	clrl	%d0
	rts
err:
	jra	PIC_PLT(CERROR)
