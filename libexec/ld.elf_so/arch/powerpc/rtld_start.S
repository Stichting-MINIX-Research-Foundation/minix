/*	$NetBSD: rtld_start.S,v 1.17 2011/09/26 01:52:22 mrg Exp $	*/

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

	.globl	_rtld_start
	.globl	_rtld

	.text

_rtld_start:
	stwu	%r1,-48(%r1)
	stw	%r3,12(%r1)		# argc
	stw	%r4,16(%r1)		# argv
	stw	%r5,20(%r1)		# envp
/*	stw	%r6,24(%r1)		# obj		(always 0) */
/*	stw	%r7,28(%r1)		# cleanup	(always 0) */
	stw	%r8,32(%r1)		# ps_strings

	bcl	20,31,1f
1:	mflr	%r30
	mr	%r3,%r30		# save for _DYNAMIC
	addis	%r30,%r30,_GLOBAL_OFFSET_TABLE_-1b@ha
	addi	%r30,%r30,_GLOBAL_OFFSET_TABLE_-1b@l
	addis	%r3,%r3,_DYNAMIC-1b@ha	# get _DYNAMIC actual address
	addi	%r3,%r3,_DYNAMIC-1b@l
	lwz	%r28,0(%r30)		# get base-relative &_DYNAMIC
	sub	%r28,%r3,%r28		# r28 = relocbase
	mr	%r4,%r28		# r4 = relocbase
	bl	_rtld_relocate_nonplt_self

	lwz	%r3,16(%r1)
	addi	%r3,%r3,-12		# sp = &argv[-3]	/* XXX */
	mr	%r4,%r28		# r4 = relocbase
	bl	_rtld			# _start = _rtld(sp, relocbase)
	mtlr	%r3

	lwz	%r3,12(%r1)		# argc
	lwz	%r4,16(%r1)		# argv
	lwz	%r5,20(%r1)		# envp
	lwz	%r6,-8(%r4)		# obj = sp[1] (== argv[-2])
	lwz	%r7,-12(%r4)		# cleanup = sp[0] (== argv[-3])
	lwz	%r8,32(%r1)		# ps_strings

	addi	%r1,%r1,48
	blrl		# _start(argc, argv, envp, obj, cleanup, ps_strings)

	li	%r0,1			# _exit()
	sc

END(_rtld_start)

	.globl	_rtld_bind

/*
 * secure-plt expects %r11 to be the offset to the rela entry.
 * bss-plt expects %r11 to be index of the rela entry.
 * So for bss-plt, we multiply the index by 12 to get the offset.
 */
ENTRY_NOPROFILE(_rtld_bind_secureplt_start)
	stwu	%r1,-160(%r1)
	stw	%r0,20(%r1)

	/*
	 * Instead of division which is costly we will use multiplicative
	 * inverse.  a / n = ((a * inv(n)) >> 32)
	 * where inv(n) = (0x100000000 + n - 1) / n
	 */
	mr	%r0,%r11
	lis	%r11,0x15555556@h	# load multiplicative inverse of 12
	ori	%r11,%r11,0x15555556@l
	mulhwu	%r11,%r11,%r0		# get high half of multiplication

	b	1f
ENTRY_NOPROFILE(_rtld_bind_bssplt_start)
	stwu	%r1,-160(%r1)

	stw	%r0,20(%r1)
1:
	mflr	%r0
	stw	%r0,16(%r1)		# save lr
	mfcr	%r0
	stw	%r0,12(%r1)		# save cr
	stmw	%r3,24(%r1)		# save r3-r31

	mr	%r3,%r12		# obj
	mr	%r4,%r11		# reloff
	bl	_rtld_bind		# _rtld_bind(obj, reloff)
	mtctr	%r3

	lmw	%r3,24(%r1)		# load r3-r31
	lwz	%r0,12(%r1)		# restore cr
	mtcr	%r0
	lwz	%r0,16(%r1)		# restore lr
	mtlr	%r0
	lwz	%r0,20(%r1)

	addi	%r1,%r1,160
	bctr
END(_rtld_bind_secureplt_start)

	.globl	_rtld_powerpc_pltcall
	.globl	_rtld_powerpc_pltresolve

_rtld_powerpc_pltcall:
	slwi	%r11,%r11,2
	addis	%r11,%r11,0		# addis	11,11,jmptab@ha
	lwz	%r11,0(%r11)		# lwz	11,jmptab@l(11)
	mtctr	%r11
	bctr

_rtld_powerpc_pltresolve:
	lis	%r12,0			# lis	12,_rtld_bind_bssplt_start@ha
	addi	%r12,%r12,0		# addi	12,12,_rtld_bind_bssplt_start@l
	mtctr	%r12
	lis	%r12,0			# lis	12,obj@ha
	addi	%r12,%r12,0		# addi	12,12,obj@l
	bctr
