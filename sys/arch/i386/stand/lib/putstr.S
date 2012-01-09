/*	$NetBSD: putstr.S,v 1.2 2008/04/28 20:23:25 martin Exp $	*/

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

/*
 * Diagnostic print routines callable from 32bit C code during early
 * parts of the boot process.
 */

/*
 * void putstr(const char *)
 *
 * display message on console
 * bugs: message address must be less than 64k
 */

	.globl	_C_LABEL(putstr)
_C_LABEL(putstr):
	.code32
	movl	4(%esp), %eax
	pusha
	movl	%eax, %esi

	call	_C_LABEL(prot_to_real)
	.code16

	movl	%esi, %eax
	call	message

	calll	_C_LABEL(real_to_prot)
	.code32
	popa
	ret

/*
 * void putint(int)
 *
 * display value on console as 8 hex digits followed by a space
 */

	.globl	_C_LABEL(putint)
_C_LABEL(putint):
	.code32
	movl	4(%esp), %eax
	pusha

	call	_C_LABEL(prot_to_real)
	.code16

	call	dump_eax

	calll	_C_LABEL(real_to_prot)
	.code32
	popa
	ret
