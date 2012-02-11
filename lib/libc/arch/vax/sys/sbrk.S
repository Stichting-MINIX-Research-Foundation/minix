/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#if defined(SYSLIBC_SCCS) && !defined(lint)
	/* .asciz "@(#)sbrk.s	8.1 (Berkeley) 6/4/93" */
	.asciz "$NetBSD: sbrk.S,v 1.10 2003/08/07 16:42:33 agc Exp $"
#endif /* SYSLIBC_SCCS and not lint */

#include "SYS.h"

	.globl	_end
	.globl	_C_LABEL(__minbrk)
	.globl	CURBRK

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

	.data
_C_LABEL(__minbrk):
	.long	_end
CURBRK:
	.long	_end
	.text

ENTRY(_sbrk, 0)
	addl3	CURBRK,4(%ap),-(%sp)
	pushl	$1
	movl	%ap,%r3
	movl	%sp,%ap
	chmk	$ SYS_break
	jcs 	err
	movl	CURBRK,%r0
	addl2	4(%r3),CURBRK
	ret
err:
	jmp	CERROR+2
