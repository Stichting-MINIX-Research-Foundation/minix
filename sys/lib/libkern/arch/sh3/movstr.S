/*	$NetBSD: movstr.S,v 1.9 2009/01/07 22:15:18 uwe Exp $	*/

/*-
 * Copyright (C) 1999 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>


#ifdef __ELF__
	.hidden __movstr, __movmem
#endif


NENTRY(__movstr)
	mov	#16, r0
	cmp/gt	r0, r6
	bf	loop2

	.align	2
loop1:
	mov.l	@r5+, r0
	mov.l	r0, @r4
	mov.l	@r5+, r0
	mov.l	r0, @(4, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(8, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(12, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(16, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(20, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(24, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(28, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(32, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(36, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(40, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(44, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(48, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(52, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(56, r4)
	mov.l	@r5+, r0
	mov.l	r0, @(60, r4)
	add	#-16, r6
	add	#64, r4

	mov	#16, r0
	cmp/gt	r0, r6
	bt	loop1

loop2:
	add	#-32, r6
	.align	2
1:
	mov.l	@r5+, r0
	mov.l	r0, @r4
	add	#4, r4
	add	#1, r6
	tst	r6, r6
	bf	1b

	rts
	 nop


/* gcc4 uses movmem, older versions use movstr */
STRONG_ALIAS(__movmem, __movstr)
