/*	$NetBSD: rtld_start.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

	.globl	_rtld_start
	.globl	_rtld

#define	CF_LEN		(3*__SIZEOF_POINTER__)
#define	CF_OBJ		(2*__SIZEOF_POINTER__)
#define	CF_CLEANUP	(1*__SIZEOF_POINTER__)

	/*
	 * void ___start(void (*cleanup)(void), const Obj_Entry *obj,
	 *    struct ps_strings *ps_strings);
	 */
ENTRY_NP(_rtld_start)
	l.sw	CF_LEN(r1), r1		# reserve some stack space
#if 0
	l.ori	r26, r3, 0		# obj		(should be 0)
	l.ori	r28, r4, 0		# cleanup	(should be 0)
#endif
	l.ori	r30, r5, 0		# ps_strings
	l.addi	r1, r1, -CF_LEN

	PIC_GOTSETUP(r16)

	l.movhi	r3, gotoffhi(_DYNAMIC)	# get _DYNAMIC gotoff address
	l.ori	r3, r3, gotofflo(_DYNAMIC)

	l.lwz	r7, 0(r16)		# get base-relative &_DYNAMIC
	l.add	r3, r3, r16		# r3 = _DYNAMIC actual address
	l.sub	r24, r3, r7		# r24 = relocbase
	l.ori	r4, r24, 0		# r4 = relocbase
	l.jal	_C_LABEL(_rtld_relocate_nonplt_self)
	l.nop

	l.addi	r3, r1, CF_CLEANUP
	l.ori	r4, r24, 0		# r4 = relocbase
	l.jal	_C_LABEL(_rtld)		# _start = _rtld(sp, relocbase)
	l.nop

	l.lwz	r3, CF_OBJ(r1)		# obj
	l.lwz	r4, CF_CLEANUP(r1)	# cleanup
	l.ori	r5, r30, 0		# ps_strings

	l.addi	r1, r1, CF_LEN		# pop stack
	l.jalr	r11			# _start(obj, cleanup, ps_strings)

	l.ori	r13, r0, 1		# _exit()
	l.sys	0

END(_rtld_start)

	.globl	_rtld_bind

/*
 * r11 = rela offset
 * r12 = GOT[1] (&obj)
 * r15 = GOT[2] (&_rtld_bind_start)
 */
ENTRY_NP(_rtld_bind_start)

	l.sw	-4(r1), r9		# save lr
	l.sw	-8(r1), r8		# save arg5
	l.sw	-12(r1), r7		# save arg4
	l.sw	-16(r1), r6		# save arg3
	l.sw	-20(r1), r5		# save arg2
	l.sw	-24(r1), r4		# save arg1
	l.sw	-28(r1), r3		# save arg0
	l.sw	-32(r1), r1		# establish stack frame
	l.addi	r1, r1, -32

	l.ori	r3, r12, 0		# obj
	l.ori	r4, r11, 0		# reloff
	l.jal	_C_LABEL(_rtld_bind)	# _rtld_bind(obj, reloff)
	l.nop

	l.addi	r1, r1, 32		# pop stack frame
	l.lwz	r3, -28(r1)		# restore arg0
	l.lwz	r4, -24(r1)		# restore arg1
	l.lwz	r5, -20(r1)		# restore arg2
	l.lwz	r6, -16(r1)		# restore arg3
	l.lwz	r7, -12(r1)		# restore arg4
	l.lwz	r8, -8(r1)		# restore arg5
	l.lwz	r9, -4(r1)		# restore lr

	l.jr	r11			# jump to routine
	l.nop
END(_rtld_bind_start)
