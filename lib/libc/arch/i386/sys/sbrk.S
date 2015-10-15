/*	$NetBSD: sbrk.S,v 1.22 2014/05/23 02:34:19 uebayasi Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)sbrk.s	5.1 (Berkeley) 4/23/90
 */

#include <machine/asm.h>
#if defined(SYSLIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: sbrk.S,v 1.22 2014/05/23 02:34:19 uebayasi Exp $")
#endif /* SYSLIBC_SCCS and not lint */

#include "SYS.h"

	.globl	_end
	.globl	CURBRK

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

	.data
CURBRK:	.long	_end
	.text

ENTRY(_sbrk)
#ifdef __PIC__
	movl	4(%esp),%ecx
	PIC_PROLOGUE
	movl	PIC_GOT(CURBRK),%edx
	PIC_EPILOGUE
	movl	(%edx),%eax
	test	%ecx,%ecx
	jz	out
	addl	%eax,%ecx
	movl	%ecx,4(%esp)
	OSYSTRAP(break)	/* don't clobber %ecx */
	jc	err
	PIC_PROLOGUE
	movl	PIC_GOT(CURBRK),%edx
	PIC_EPILOGUE
	movl	(%edx),%eax
	movl	%ecx,(%edx)
out:
	ret
err:
	jmp CERROR
#else
	movl	4(%esp),%ecx
	movl	CURBRK,%eax
	test	%ecx,%ecx
	jz	out
	addl	%eax,4(%esp)
	OSYSTRAP(break)	/* don't clobber %ecx */
	jc	err
	movl	CURBRK,%eax
	addl	%ecx,CURBRK
out:
	ret
err:
	jmp	CERROR
#endif
END(_sbrk)
