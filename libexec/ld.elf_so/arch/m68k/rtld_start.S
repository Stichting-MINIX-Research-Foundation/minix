/*	$NetBSD: rtld_start.S,v 1.11 2014/03/18 18:20:38 riastradh Exp $	*/

/*-
 * Copyright (c) 1999, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein and by Charles M. Hannum.
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

	.text
	.align	4
	.globl	.rtld_start
	.type	.rtld_start,@function
.rtld_start:
	subql	#8,%sp			| storage for obj and cleanup

	GOT_SETUP(%a0)
	LEA_LCL(_DYNAMIC,%a1)

	movel	%a1,%a5
	subl	(%a0),%a5

	movel	%a5,-(%sp)		| relocbase
	movel	%a1,-(%sp)		| &_DYNAMIC
	jbsr	_rtld_relocate_nonplt_self@PLTPC
	addql	#8,%sp			| pop arguments

	movel	%a5,-(%sp)		| relocbase
	pea	4(%sp)			| sp
	jbsr	_rtld@PLTPC		| entry = _rtld(sp, relocbase)
	addql	#8,%sp			| pop arguments

	moveal	(%sp)+,%a1		| cleanup
	moveal	(%sp)+,%a0		| obj
	moveal	%d0,%a3
	jmp	(%a3)			| Go for it!
	.size	.rtld_start,.-.rtld_start

	.align	4
	.globl	_rtld_bind_start
	.type	_rtld_bind_start,@function
_rtld_bind_start:
	INTERRUPT_SAVEREG
	movel	20(%sp),-(%sp)		| push reloff
	movel	(16+4)(%sp),-(%sp)	| push obj
	jbsr	_rtld_bind@PLTPC	| %a0 = _rtld_bind(obj, reloff)
	addql	#8,%sp			| pop args
#ifdef __SVR4_ABI__
	movel	%a0,(16+4)(%sp)		| write fake `return' address over obj
#else
	movel	%d0,(16+4)(%sp)		| write fake `return' address over obj
#endif
	INTERRUPT_RESTOREREG		| restore caller-saved registers
	addql	#4,%sp			| skip reloff
	rts				| `return' right into function
	.size	_rtld_bind_start,.-_rtld_bind_start
