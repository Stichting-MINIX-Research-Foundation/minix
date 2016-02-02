/*	$NetBSD: memset.S,v 1.5 2014/05/22 16:47:31 pooka Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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
	RCSID("$NetBSD: memset.S,v 1.5 2014/05/22 16:47:31 pooka Exp $")
#endif

#ifndef _KERNEL
/* bzero, %rdi is buffer, %rsi length */

ENTRY(bzero)
	mov	%rsi,%rdx		/* length */
	xor	%eax,%eax		/* value to write */
	jmp	1f
#endif

/* memset, %rdi is buffer, %rsi char to fill, %rdx length */

ENTRY(memset)
	movzbq	%sil,%rax		/* byte value to fill */
	mov	%rdx,%rsi		/* copy of length */
	mov	$0x0101010101010101,%r9
	imul	%r9,%rax		/* fill value in all bytes */

1:
	mov	%rdi,%r9		/* Need to return buffer address */
	or	%edi,%edx		/* address | length */
	mov	%rsi,%rcx
	cmp	$7,%rsi
	jbe	10f			/* jump if short fill */
	test	$7,%dl			/* check for misaligned fill */
	jnz	20f			/* jump if misaligned */

/* Target aligned and length multiple of 8 */
2:
	shr	$3,%rcx
	rep	stosq
	mov	%r9,%rax
	ret

/*
 * Short transfer, any faffing here will generate mispredicted branches.
 * So we keep it simple.
 */
10:	rep	stosb
	mov	%r9,%rax
	ret

/*
 * Buffer or length misaligned.
 * Write pattern to first and last word of buffer, then fill middle.
 * (This writes to some bytes more than once - possibly three times!.)
 */
20:
	mov	%rax,(%rdi)
	movzbq	%dil,%rdx		/* low address for alignment */
	mov	%rax,-8(%rcx,%rdi)
	and	$7,%dl			/* offset in word */
	sub	%rdx,%rcx		/* adjust length ... */
	add	%rdx,%rdi		/* ... and target */
	jmp	2b
END(memset)

#ifndef _KERNEL
END(bzero)
#endif
