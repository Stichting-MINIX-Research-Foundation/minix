/*	$NetBSD: memset.S,v 1.4 2008/04/29 06:53:01 martin Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
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

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: memset.S,v 1.4 2008/04/29 06:53:01 martin Exp $")
#endif

#ifdef BZERO
ENTRY(bzero)
#else
ENTRY(memset)
#endif
#ifdef BZERO
	movl	8(%esp),%ecx
	xor	%eax,%eax
#else
	movl	12(%esp),%ecx
	movzbl	8(%esp),%eax		/* unsigned char, zero extend */
#endif
	cmpl	$0x0f,%ecx		/* avoid mispredicted branch... */

	pushl	%edi
	movl	8(%esp),%edi

	/*
	 * if the string is too short, it's really not worth the overhead
	 * of aligning to word boundries, etc.  So we jump to a plain
	 * unaligned set.
	 *
	 * NB aligning the transfer is actually pointless on my athlon 700,
	 * It does make a difference to a PII though.
	 *
	 * The PII, PIII and PIV all seem to have a massive performance
	 * drop when the initial target address is an odd multiple of 4.
	 */
	jbe	.Lby_bytes

#ifndef BZERO
	movb	%al,%ah			/* copy char to all bytes in word */
	movl	%eax,%edx
	sall	$16,%eax
	orl	%edx,%eax
#endif

	movl	%edi,%edx		/* detect misalignment */
	neg	%edx
	andl	$7,%edx
	jnz	.Lalign
.Laligned:
	movl	%eax,-4(%edi,%ecx)	/* zap last 4 bytes */
	shrl	$2,%ecx			/* zero by words */
	rep
	stosl
.Ldone:
#ifndef BZERO
	movl	8(%esp),%eax		/* return address of buffer */
#endif
	pop	%edi
	ret

.Lalign:
	movl	%eax,(%edi)		/* zap first 8 bytes */
	movl	%eax,4(%edi)
	subl	%edx,%ecx		/* remove from main count */
	add	%edx,%edi
	jmp	.Laligned

.Lby_bytes:
	rep
	stosb

#ifndef BZERO
	movl	8(%esp),%eax		/* return address of buffer */
#endif
	popl	%edi
	ret
