/*	$NetBSD: movstrSI.S,v 1.8 2009/01/07 22:15:18 uwe Exp $	*/

/*
 * Copyright (c) 2000 SHIMIZU Ryo.  All rights reserved.
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
	.hidden __movstrSI4, __movmemSI4 
	.hidden __movstrSI8, __movmemSI8 
	.hidden __movstrSI12, __movmemSI12
	.hidden __movstrSI16, __movmemSI16
	.hidden __movstrSI20, __movmemSI20
	.hidden __movstrSI24, __movmemSI24
	.hidden __movstrSI28, __movmemSI28
	.hidden __movstrSI32, __movmemSI32
	.hidden __movstrSI36, __movmemSI36
	.hidden __movstrSI40, __movmemSI40
	.hidden __movstrSI44, __movmemSI44
	.hidden __movstrSI48, __movmemSI48
	.hidden __movstrSI52, __movmemSI52
	.hidden __movstrSI56, __movmemSI56
	.hidden __movstrSI60, __movmemSI60
	.hidden __movstrSI64, __movmemSI64
#endif


NENTRY(__movstrSI64)
	mov.l	@(60, r5), r0
	mov.l	r0, @(60, r4)
ALTENTRY(__movstrSI60)
	mov.l	@(56, r5), r0
	mov.l	r0, @(56, r4)
ALTENTRY(__movstrSI56)
	mov.l	@(52, r5), r0
	mov.l	r0, @(52, r4)
ALTENTRY(__movstrSI52)
	mov.l	@(48, r5), r0
	mov.l	r0, @(48, r4)
ALTENTRY(__movstrSI48)
	mov.l	@(44, r5), r0
	mov.l	r0, @(44, r4)
ALTENTRY(__movstrSI44)
	mov.l	@(40, r5), r0
	mov.l	r0, @(40, r4)
ALTENTRY(__movstrSI40)
	mov.l	@(36, r5), r0
	mov.l	r0, @(36, r4)
ALTENTRY(__movstrSI36)
	mov.l	@(32, r5), r0
	mov.l	r0, @(32, r4)
ALTENTRY(__movstrSI32)
	mov.l	@(28, r5), r0
	mov.l	r0, @(28, r4)
ALTENTRY(__movstrSI28)
	mov.l	@(24, r5), r0
	mov.l	r0, @(24, r4)
ALTENTRY(__movstrSI24)
	mov.l	@(20, r5), r0
	mov.l	r0, @(20, r4)
ALTENTRY(__movstrSI20)
	mov.l	@(16, r5), r0
	mov.l	r0, @(16, r4)
ALTENTRY(__movstrSI16)
	mov.l	@(12, r5), r0
	mov.l	r0, @(12, r4)
ALTENTRY(__movstrSI12)
	mov.l	@(8, r5), r0
	mov.l	r0, @(8, r4)
ALTENTRY(__movstrSI8)
	mov.l	@(4, r5), r0
	mov.l	r0, @(4, r4)
ALTENTRY(__movstrSI4)
	mov.l	@r5, r0
	rts
	 mov.l	r0, @r4


/* gcc4 uses movmem, older versions use movstr */
STRONG_ALIAS(__movmemSI4, __movstrSI4)
STRONG_ALIAS(__movmemSI8, __movstrSI8)
STRONG_ALIAS(__movmemSI12, __movstrSI12)
STRONG_ALIAS(__movmemSI16, __movstrSI16)
STRONG_ALIAS(__movmemSI20, __movstrSI20)
STRONG_ALIAS(__movmemSI24, __movstrSI24)
STRONG_ALIAS(__movmemSI28, __movstrSI28)
STRONG_ALIAS(__movmemSI32, __movstrSI32)
STRONG_ALIAS(__movmemSI36, __movstrSI36)
STRONG_ALIAS(__movmemSI40, __movstrSI40)
STRONG_ALIAS(__movmemSI44, __movstrSI44)
STRONG_ALIAS(__movmemSI48, __movstrSI48)
STRONG_ALIAS(__movmemSI52, __movstrSI52)
STRONG_ALIAS(__movmemSI56, __movstrSI56)
STRONG_ALIAS(__movmemSI60, __movstrSI60)
STRONG_ALIAS(__movmemSI64, __movstrSI64)
