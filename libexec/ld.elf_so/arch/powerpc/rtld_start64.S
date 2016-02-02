/*	$NetBSD: rtld_start64.S,v 1.2 2014/03/06 19:19:40 matt Exp $	*/

/*-
 * Copyright (C) 1998	Tsubai Masanari
 * Portions copyright 2002 Charles M. Hannum <root@ihack.net>
 * All rights reserved.
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

	.globl	_rtld_relocate_nonplt_self
	.globl	_rtld

	.text
ENTRY_NOPROFILE(_rtld_start)
	mr	%r23,%r3		// argc
	mr	%r24,%r4		// argv
	mr	%r25,%r5		// envp
/*	mr	%r26,%r6		// obj		(always 0) */
/*	mr	%r27,%r7		// cleanup	(always 0) */
	mr	%r28,%r8		// ps_strings
	li	%r0,0
	stdu	%r0,-64(%r1)		// terminate stack chain
	std	%r2,40(%r1)		// save TOC
	std	%r0,16(%r1)		// ditto

	bcl	20,31,1f
1:	mflr	%r30
	ld	%r3,0(%r2)		// TOC[0] = &TOC
	sub	%r29,%r2,%r3		// compute relocbase

	addis	%r3,%r3,_DYNAMIC-1b@ha	// get _DYNAMIC actual address
	addi	%r3,%r3,_DYNAMIC-1b@l
	mr	%r4,%r29		// r4 = relocbase
	CALL(_rtld_relocate_nonplt_self)

	addi	%r3,%r1,48		// sp = <local variable space>
	mr	%r4,%r29		// r4 = relocbase
	CALL(_rtld)			// _start = _rtld(sp, relocbase)

	ld	%r0,0(%r3)		// func address
	ld	%r2,8(%r3)		// TOC address
	ld	%r11,16(%r3)		// environment pointer
	mtctr	%r0			// so we can call it.

	mr	%r3,%r23		// argc
	mr	%r4,%r24		// argv
	mr	%r5,%r25		// envp
	ld	%r6,56(%r1)		// obj = <localvar>[1]
	ld	%r7,48(%r1)		// cleanup = <localvar>[0]
	mr	%r8,%r28		// ps_strings

	bctrl		// _start(argc, argv, envp, obj, cleanup, ps_strings)
	nop

	li	%r0,1			// _exit()
	sc
END(_rtld_start)

	.globl	_rtld_bind

/*
 * %r0 has the index of the rela, %r12 has a pointer to the plt entry.
 */
ENTRY_NOPROFILE(_rtld_bind_start)
	std	%r3,-72(%r1)		// save argument register
	std	%r4,-64(%r1)		// save argument register
	std	%r5,-56(%r1)		// save argument register
	std	%r6,-48(%r1)		// save argument register
	std	%r7,-40(%r1)		// save argument register
	std	%r8,-32(%r1)		// save argument register
	std	%r9,-24(%r1)		// save argument register
	std	%r10,-16(%r1)		// save argument register
	std	%r31,-8(%r1)		// save register
	stdu	%r1,-(48+80)(%r1)	// create back chain
	mflr	%r10
	std	%r10,16(%r1)		// save LR
	mfcr	%r9
	std	%r9,8(%r1)		// save CR to be safe

	mr	%r31, %r12		// save this across bind call
	mr	%r3, %r11 		// obj
	mr	%r4, %r0		// reloff

	CALL(_rtld_bind)		// _rtld_bind(obj, reloff)

	mtctr	%r3
	mr	%r12,%r31		// restore r12

	ld	%r0,8(%r1)		// get saved CR
	mtcr	%r0			// restore it
	ld	%r0,16(%r1)		// get saved LR
	mtlr	%r0			// restore it

	addi	%r1,%r1,(48+80)		// adjust stack
	ld	%r3,-72(%r1)		// restore argument register
	ld	%r4,-64(%r1)		// restore argument register
	ld	%r5,-56(%r1)		// restore argument register
	ld	%r6,-48(%r1)		// restore argument register
	ld	%r7,-40(%r1)		// restore argument register
	ld	%r8,-32(%r1)		// restore argument register
	ld	%r9,-24(%r1)		// restore argument register
	ld	%r10,-16(%r1)		// restore argument register
	ld	%r31,-8(%r1)		// restore register
	bctr
END(_rtld_bind_start)
