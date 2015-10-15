/*	$NetBSD: atomic_op_asm.h,v 1.6 2014/03/07 07:17:54 matt Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _ATOMIC_OP_ASM_H_
#define	_ATOMIC_OP_ASM_H_

#include <machine/asm.h>

#if defined(_KERNEL)

#define	ATOMIC_OP_ALIAS(a,s)	STRONG_ALIAS(a,s)

#else /* _KERNEL */

#define	ATOMIC_OP_ALIAS(a,s)	WEAK_ALIAS(a,s)

#endif /* _KERNEL */

#define	ATOMIC_OP_32_ARG(op,insn,arg) \
ENTRY(_atomic_##op##_32)	; \
	mr	%r10,%r3	; \
1:	lwarx	%r3,0,%r10	; \
	insn	%r5,%r3,arg	; \
	stwcx.	%r5,0,%r10	; \
	beqlr+			; \
	b	1b		; \
END(_atomic_##op##_32)		; \
ATOMIC_OP_ALIAS(atomic_##op##_32,_atomic_##op##_32)

#define	ATOMIC_OP_64_ARG(op,insn,arg) \
ENTRY(_atomic_##op##_64)	; \
	mr	%r10,%r3	; \
1:	ldarx	%r3,0,%r10	; \
	insn	%r5,%r3,arg	; \
	stdcx.	%r5,0,%r10	; \
	beqlr+			; \
	b	1b		; \
END(_atomic_##op##_64)		; \
ATOMIC_OP_ALIAS(atomic_##op##_64,_atomic_##op##_64)

#define	ATOMIC_OP_32_ARG_NV(op,insn,arg) \
ENTRY(_atomic_##op##_32_nv)	; \
	mr	%r10,%r3	; \
1:	lwarx	%r3,0,%r10	; \
	insn	%r3,%r3,arg	; \
	stwcx.	%r3,0,%r10	; \
	beqlr+			; \
	b	1b		; \
END(_atomic_##op##_32_nv)	; \
ATOMIC_OP_ALIAS(atomic_##op##_32_nv,_atomic_##op##_32_nv)

#define	ATOMIC_OP_64_ARG_NV(op,insn,arg) \
ENTRY(_atomic_##op##_64_nv)	; \
	mr	%r10,%r3	; \
1:	ldarx	%r3,0,%r10	; \
	insn	%r3,%r3,arg	; \
	stdcx.	%r3,0,%r10	; \
	beqlr+			; \
	b	1b		; \
END(_atomic_##op##_64_nv)	; \
ATOMIC_OP_ALIAS(atomic_##op##_64_nv,_atomic_##op##_64_nv)

#define ATOMIC_OP_32(op)	ATOMIC_OP_32_ARG(op,op,%r4)
#define ATOMIC_OP_32_NV(op)	ATOMIC_OP_32_ARG_NV(op,op,%r4)

#define ATOMIC_OP_64(op)	ATOMIC_OP_64_ARG(op,op,%r4)
#define ATOMIC_OP_64_NV(op)	ATOMIC_OP_64_ARG_NV(op,op,%r4)

#endif /* _ATOMIC_OP_ASM_H_ */
