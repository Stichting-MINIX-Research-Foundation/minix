/*	$NetBSD: sbrk.S,v 1.18 2010/12/12 09:03:06 tsutsui Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
	RCSID("from: @(#)sbrk.s	8.1 (Berkeley) 6/4/93")
#else
	RCSID("$NetBSD: sbrk.S,v 1.18 2010/12/12 09:03:06 tsutsui Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

	.globl	_C_LABEL(__curbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

	.data
_C_LABEL(__curbrk):
	PTR_WORD	_C_LABEL(_end)
	.text

LEAF(_sbrk)
	PIC_PROLOGUE(_sbrk)
	PTR_LA		t0, _C_LABEL(__curbrk)
	PTR_L		v1, 0(t0)
	PTR_ADDU	a0, a0, v1	# compute current break
	SYSTRAP(break)
	bne		a3, zero, 1f
	move		v0, v1		# return old val of curbrk from above
	PTR_LA		t0, _C_LABEL(__curbrk)
	PTR_S		a0, 0(t0)	# save current val of curbrk from above
	PIC_RETURN()
1:
	PIC_TAILCALL(__cerror)
END(_sbrk)
