/*	$NetBSD: strlcpy.S,v 1.3 2008/04/28 20:22:56 martin Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthew Fredette.
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
 
#if defined(LIBC_SCCS) && !defined(lint) 
        RCSID("$NetBSD: strlcpy.S,v 1.3 2008/04/28 20:22:56 martin Exp $")
#endif /* LIBC_SCCS and not lint */

/*
 * size_t strlcpy(char *dst, const char *src, size_t size);
 */
LEAF_ENTRY(_strlcpy)
	.weak strlcpy
	strlcpy = _strlcpy

	/* Always load the first byte of the source string. */
	ldbs,ma		1(%arg1), %r1

	/*
	 * If our dst buffer is zero bytes, branch immediately
	 * to the code that counts the (remaining) length
	 * of the src string.
	 */
	comb,=		%r0, %arg2, $strlcpy_dst_done
	copy		%arg1, %ret0

	/*
	 * If our dst buffer is one byte, branch immediately 
	 * to the code that NUL-terminates the dst buffer and
	 * counts the (remaining) length of the src string.
	 */
	addib,=,n	-1, %arg2, $strlcpy_dst_full

	/*
	 * Loop copying bytes.  At the top of this loop,
	 * %arg2 is always the number of bytes we can still 
	 * store, and %r1 holds the next byte to store.
	 */
$strlcpy_loop:
	comb,=		%r0, %r1, $strlcpy_exit
	stbs,ma		%r1, 1(%arg0)
	addib,<>	-1, %arg2, $strlcpy_loop
	ldbs,ma		1(%arg1), %r1

$strlcpy_dst_full:
	stbs,ma		%r0, 1(%arg0)

$strlcpy_dst_done:
	comb,<>,n	%r0, %r1, $strlcpy_dst_done
	ldbs,ma		1(%arg1), %r1
	
$strlcpy_exit:
	bv		%r0(%rp)
	sub		%arg1, %ret0, %ret0
EXIT(strlcpy)
