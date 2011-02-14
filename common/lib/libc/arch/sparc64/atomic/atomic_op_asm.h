/*	$NetBSD: atomic_op_asm.h,v 1.6 2011/01/17 18:11:10 joerg Exp $	*/

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

#define	ATOMIC_OP_ALIAS(a,s)		STRONG_ALIAS(a,s)

#ifdef __arch64__
#define	ATOMIC_OP_ALIAS_SIZE(a,s)	STRONG_ALIAS(a,s ## _64)
#else
#define	ATOMIC_OP_ALIAS_SIZE(a,s)	STRONG_ALIAS(a,s ## _32)
#endif

#else /* _KERNEL */

#define	ATOMIC_OP_ALIAS(a,s)		WEAK_ALIAS(a,s)

#ifdef __arch64__
#define	ATOMIC_OP_ALIAS_SIZE(a,s)	WEAK_ALIAS(a,s ## _64)
#else
#define	ATOMIC_OP_ALIAS_SIZE(a,s)	WEAK_ALIAS(a,s ## _32)
#endif

#endif /* _KERNEL */

#ifdef __arch64__
#define	STRONG_ALIAS_SIZE(a,s)		STRONG_ALIAS(a,s ## _64)
#else
#define	STRONG_ALIAS_SIZE(a,s)		STRONG_ALIAS(a,s ## _32)
#endif

#endif /* _ATOMIC_OP_ASM_H_ */
