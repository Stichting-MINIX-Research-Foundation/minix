/*	$NetBSD: memchr.S,v 1.6 2014/03/22 19:16:34 jakllsch Exp $	*/

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
	RCSID("$NetBSD: memchr.S,v 1.6 2014/03/22 19:16:34 jakllsch Exp $")
#endif

/*
 * The instruction sequences used try to avoid data dependencies
 * between adjacent instructions (to allow parallel execution).
 * The 'imul' for %r9 could be put into the delay following the
 * memory read (ie inside the loop) at no obvious cost - except
 * that the loop is currently exactly 32 bytes - 2 fetch blocks!.
 *
 * I don't think aligning any of the other branch targets is useful.
 */

ENTRY(memchr)
	movabsq	$0x0101010101010101,%r8
	lea	(%rdi,%rdx),%r10	/* limit of buffer to scan */
	movzbq	%sil,%rsi	/* mask high bits! */

	/* 'directpath' imuls can execute 3 at a time ... (amd) */
	imul	%r8,%rsi	/* search byte replicated in word */
	imul	$0x80,%r8,%r9	/* 0x8080808080808080 */
	test	$7,%dil
	jnz	20f		/* jump if misaligned */
	jmp	1f		/* jump to avoid 4 nops (13 bytes) in gap */

	_ALIGN_TEXT		/* entire loop now in 32 aligned bytes */
1:
	cmpq	%r10,%rdi	/* end of buffer ? */
	jae	30f		/* jump if so */

	movq	(%rdi),%rax	/* value to check */
	addq	$8,%rdi
	xorq	%rsi,%rax	/* now looking for zeros */
2:
	mov	%rax,%rcx
	subq	%r8,%rax	/* x - 0x01 */
	not	%rcx
	andq	%r9,%rax	/* (x - 0x01) & 0x80 */
	andq	%rcx,%rax	/* ((x - 0x01) & 0x80) & ~x */
	je	1b		/* jump if not found */

/* Found byte in word, get its address */
	bsf	%rax,%rax
	shr	$3,%eax
	lea	-8(%rax,%rdi),%rax
	cmpq	%r10,%rax	/* need to check not beyond buffer */
	jae	30f
	rep
	ret			/* amd - no ret after jmp */

/* Input misaligned, read aligned and make low bytes invalid */
20:
	mov	%dil,%cl	/* misalignment amount 1..7 (+high bits )*/
	and	$~7,%dil	/* %rdi now start of word */
	test	%rdx,%rdx	/* zero length, don't read */
	jz	30f

	neg	%cl		/* 7..1 (+high bits) */
	mov	(%rdi),%rax	/* word containing first byte */
	addq	$8,%rdi
	and	$7,%cl		/* 7..1 */

	mov	%r8,%r11	/* any value with bits in each byte */
	shl	$3,%cl		/* 56..8 */
	xorq	%rsi,%rax	/* now looking for zeros */

	/* Set low bytes non-zero */
	shr	%cl,%r11	/* non-zero in unwanted bytes */
	or	%r11,%rax	/* low bytes now set */
	jmp	2b

/* Not found */
30:	xorq	%rax,%rax
	ret
END(memchr)
