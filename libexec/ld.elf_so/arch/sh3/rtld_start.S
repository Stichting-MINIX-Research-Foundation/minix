/*	$NetBSD: rtld_start.S,v 1.7 2008/04/28 20:23:03 martin Exp $	*/

/*-
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Marcus Comstedt.
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
	.align	2
	.globl	.rtld_start
	.type	.rtld_start,@function

.rtld_start:
	mova	2f,r0
	mov.l	@r0,r2
	add	r0,r2		/* GOT */
	mov.l	@(4,r0),r4	/* _DYNAMIC@GOTOFF */
	add	r2,r4		/* _DYNAMIC */
	mov.l	@(12,r0),r1	/* _rtld_relocate_nonplt_self offset */
	mov.l	@(8,r0),r0	/* _DYNAMIC@GOT */
	mov	r4,r5
	mov.l	@(r0,r2),r0	/* where linker thinks _DYNAMIC is */
	sub	r0,r5		/* compute relocation base */
	bsrf	r1		/* _rtld_relocate_nonplt_self(dynp, relocbase) */
	 mov.l	r5,@-r15	/* save relocbase */

4:
	mov.l	@r15+,r5	/* restore relocbase */
	add	#-8,r15		/* room for values returned by _rtld */
	mov	r15,r4
	mov.l	1f,r0
	bsrf	r0		/* _rtld(sp, relocbase) */
	 mov.l	r9,@-r15	/* save ps_strings */
3:
	mov.l	@r15+,r9	/* restore ps_strings */

	mov.l	@r15+,r7	/* from _rtld: exit procedure */
	mov.l	@r15+,r8	/* from _rtld: main object */

	mov.l	@r15,r4		/* restore argc */

	mov	r15,r5		/* restore argv */
	add	#4,r5

	mov	r4,r6		/* restore envp */
	shll2	r6
	add	r15,r6
	jmp	@r0		/* entry point returned by _rtld */
	 add	#8,r6
	.align	2
1:	.long	_rtld-3b
2:	.long	_GLOBAL_OFFSET_TABLE_
	.long	_DYNAMIC@GOTOFF
	.long	_DYNAMIC@GOT
	.long	_rtld_relocate_nonplt_self-4b
	.size	.rtld_start,.-.rtld_start

	.align	2
	.globl	_rtld_bind_start
	.type	_rtld_bind_start,@function
_rtld_bind_start:		/* r0 = obj, r1 = reloff */
	mov.l	r2,@-r15	/* save registers */
	mov.l	r3,@-r15
	mov.l	r4,@-r15
	mov.l	r5,@-r15
	mov.l	r6,@-r15
	mov.l	r7,@-r15
	sts.l	mach,@-r15
	sts.l	macl,@-r15
	sts.l	pr,@-r15

	mov	r0,r4		/* copy of obj */
	mov.l	2f,r0
	bsrf	r0		/* call the binder */
	 mov	r1,r5		/* copy of reloff */
4:
	lds.l	@r15+,pr	/* restore registers */
	lds.l	@r15+,macl
	lds.l	@r15+,mach
	mov.l	@r15+,r7
	mov.l	@r15+,r6
	mov.l	@r15+,r5
	mov.l	@r15+,r4
	mov.l	@r15+,r3
	jmp	@r0
	 mov.l	@r15+,r2
	.align	2
2:	.long	_rtld_bind-4b
	.size	_rtld_bind_start,.-_rtld_bind_start

	.end
