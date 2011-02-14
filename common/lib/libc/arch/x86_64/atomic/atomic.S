/*	$NetBSD: atomic.S,v 1.16 2011/01/12 23:12:10 joerg Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe, and by Andrew Doran.
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

#include <sys/param.h>
#include <machine/asm.h>

#ifdef _KERNEL
#define	ALIAS(f, t)	STRONG_ALIAS(f,t)
#else
#define	ALIAS(f, t)	WEAK_ALIAS(f,t)
#endif

#ifdef _HARDKERNEL
#define	LOCK(n)		.Lpatch ## n:	lock
#define	ENDLABEL(a)	_ALIGN_TEXT; LABEL(a)
#else
#define	LOCK(n)		lock
#define	ENDLABEL(a)	/* nothing */
#endif

	.text

/* 32-bit */

ENTRY(_atomic_add_32)
	LOCK(1)
	addl	%esi, (%rdi)
	ret

ENTRY(_atomic_add_32_nv)
	movl	%esi, %eax
	LOCK(2)
	xaddl	%eax, (%rdi)
	addl	%esi, %eax
	ret

ENTRY(_atomic_and_32)
	LOCK(3)
	andl	%esi, (%rdi)
	ret

ENTRY(_atomic_and_32_nv)
	movl	(%rdi), %eax
1:
	movl	%eax, %ecx
	andl	%esi, %ecx
	LOCK(4)
	cmpxchgl %ecx, (%rdi)
	jnz	1b
	movl	%ecx, %eax
	ret

ENTRY(_atomic_dec_32)
	LOCK(5)
	decl	(%rdi)
	ret

ENTRY(_atomic_dec_32_nv)
	movl	$-1, %eax
	LOCK(6)
	xaddl	%eax, (%rdi)
	decl	%eax
	ret

ENTRY(_atomic_inc_32)
	LOCK(7)
	incl	(%rdi)
	ret

ENTRY(_atomic_inc_32_nv)
	movl	$1, %eax
	LOCK(8)
	xaddl	%eax, (%rdi)
	incl	%eax
	ret

ENTRY(_atomic_or_32)
	LOCK(9)
	orl	%esi, (%rdi)
	ret

ENTRY(_atomic_or_32_nv)
	movl	(%rdi), %eax
1:
	movl	%eax, %ecx
	orl	%esi, %ecx
	LOCK(10)
	cmpxchgl %ecx, (%rdi)
	jnz	1b
	movl	%ecx, %eax
	ret

ENTRY(_atomic_swap_32)
	movl	%esi, %eax
	xchgl	%eax, (%rdi)
	ret

ENTRY(_atomic_cas_32)
	movl	%esi, %eax
	LOCK(12)
	cmpxchgl %edx, (%rdi)
	/* %eax now contains the old value */
	ret

ENTRY(_atomic_cas_32_ni)
	movl	%esi, %eax
	cmpxchgl %edx, (%rdi)
	/* %eax now contains the old value */
	ret

/* 64-bit */

ENTRY(_atomic_add_64)
	LOCK(13)
	addq	%rsi, (%rdi)
	ret

ENTRY(_atomic_add_64_nv)
	movq	%rsi, %rax
	LOCK(14)
	xaddq	%rax, (%rdi)
	addq	%rsi, %rax
	ret

ENTRY(_atomic_and_64)
	LOCK(15)
	andq	%rsi, (%rdi)
	ret

ENTRY(_atomic_and_64_nv)
	movq	(%rdi), %rax
1:
	movq	%rax, %rcx
	andq	%rsi, %rcx
	LOCK(16)
	cmpxchgq %rcx, (%rdi)
	jnz	1b
	movq	%rcx, %rax
	ret

ENTRY(_atomic_dec_64)
	LOCK(17)
	decq	(%rdi)
	ret

ENTRY(_atomic_dec_64_nv)
	movq	$-1, %rax
	LOCK(18)
	xaddq	%rax, (%rdi)
	decq	%rax
	ret

ENTRY(_atomic_inc_64)
	LOCK(19)
	incq	(%rdi)
	ret

ENTRY(_atomic_inc_64_nv)
	movq	$1, %rax
	LOCK(20)
	xaddq	%rax, (%rdi)
	incq	%rax
	ret

ENTRY(_atomic_or_64)
	LOCK(21)
	orq	%rsi, (%rdi)
	ret

ENTRY(_atomic_or_64_nv)
	movq	(%rdi), %rax
1:
	movq	%rax, %rcx
	orq	%rsi, %rcx
	LOCK(22)
	cmpxchgq %rcx, (%rdi)
	jnz	1b
	movq	%rcx, %rax
	ret

ENTRY(_atomic_swap_64)
	movq	%rsi, %rax
	xchgq	%rax, (%rdi)
	ret

ENTRY(_atomic_cas_64)
	movq	%rsi, %rax
	LOCK(24)
	cmpxchgq %rdx, (%rdi)
	/* %eax now contains the old value */
	ret

ENTRY(_atomic_cas_64_ni)
	movq	%rsi, %rax
	cmpxchgq %rdx, (%rdi)
	/* %eax now contains the old value */
	ret

/* memory barriers */

ENTRY(_membar_consumer)
	LOCK(25)
	addq	$0, -8(%rsp)
	ret
ENDLABEL(membar_consumer_end)

ENTRY(_membar_producer)
	/* A store is enough */
	movq	$0, -8(%rsp)
	ret
ENDLABEL(membar_producer_end)

ENTRY(_membar_sync)
	LOCK(26)
	addq	$0, -8(%rsp)
	ret
ENDLABEL(membar_sync_end)

#ifdef _HARDKERNEL
ENTRY(sse2_lfence)
	lfence
	ret
ENDLABEL(sse2_lfence_end)

ENTRY(sse2_mfence)
	mfence
	ret
ENDLABEL(sse2_mfence_end)

atomic_lockpatch:
	.globl	atomic_lockpatch
	.quad	.Lpatch1, .Lpatch2, .Lpatch3, .Lpatch4, .Lpatch5
	.quad	.Lpatch6, .Lpatch7, .Lpatch8, .Lpatch9, .Lpatch10
	.quad	.Lpatch12, .Lpatch13, .Lpatch14, .Lpatch15
	.quad	.Lpatch16, .Lpatch17, .Lpatch18, .Lpatch19, .Lpatch20
	.quad	.Lpatch21, .Lpatch22, .Lpatch24, .Lpatch25
	.quad	.Lpatch26, 0
#endif	/* _HARDKERNEL */

ALIAS(atomic_add_32,_atomic_add_32)
ALIAS(atomic_add_64,_atomic_add_64)
ALIAS(atomic_add_int,_atomic_add_32)
ALIAS(atomic_add_long,_atomic_add_64)
ALIAS(atomic_add_ptr,_atomic_add_64)

ALIAS(atomic_add_32_nv,_atomic_add_32_nv)
ALIAS(atomic_add_64_nv,_atomic_add_64_nv)
ALIAS(atomic_add_int_nv,_atomic_add_32_nv)
ALIAS(atomic_add_long_nv,_atomic_add_64_nv)
ALIAS(atomic_add_ptr_nv,_atomic_add_64_nv)

ALIAS(atomic_and_32,_atomic_and_32)
ALIAS(atomic_and_64,_atomic_and_64)
ALIAS(atomic_and_uint,_atomic_and_32)
ALIAS(atomic_and_ulong,_atomic_and_64)
ALIAS(atomic_and_ptr,_atomic_and_64)

ALIAS(atomic_and_32_nv,_atomic_and_32_nv)
ALIAS(atomic_and_64_nv,_atomic_and_64_nv)
ALIAS(atomic_and_uint_nv,_atomic_and_32_nv)
ALIAS(atomic_and_ulong_nv,_atomic_and_64_nv)
ALIAS(atomic_and_ptr_nv,_atomic_and_64_nv)

ALIAS(atomic_dec_32,_atomic_dec_32)
ALIAS(atomic_dec_64,_atomic_dec_64)
ALIAS(atomic_dec_uint,_atomic_dec_32)
ALIAS(atomic_dec_ulong,_atomic_dec_64)
ALIAS(atomic_dec_ptr,_atomic_dec_64)

ALIAS(atomic_dec_32_nv,_atomic_dec_32_nv)
ALIAS(atomic_dec_64_nv,_atomic_dec_64_nv)
ALIAS(atomic_dec_uint_nv,_atomic_dec_32_nv)
ALIAS(atomic_dec_ulong_nv,_atomic_dec_64_nv)
ALIAS(atomic_dec_ptr_nv,_atomic_dec_64_nv)

ALIAS(atomic_inc_32,_atomic_inc_32)
ALIAS(atomic_inc_64,_atomic_inc_64)
ALIAS(atomic_inc_uint,_atomic_inc_32)
ALIAS(atomic_inc_ulong,_atomic_inc_64)
ALIAS(atomic_inc_ptr,_atomic_inc_64)

ALIAS(atomic_inc_32_nv,_atomic_inc_32_nv)
ALIAS(atomic_inc_64_nv,_atomic_inc_64_nv)
ALIAS(atomic_inc_uint_nv,_atomic_inc_32_nv)
ALIAS(atomic_inc_ulong_nv,_atomic_inc_64_nv)
ALIAS(atomic_inc_ptr_nv,_atomic_inc_64_nv)

ALIAS(atomic_or_32,_atomic_or_32)
ALIAS(atomic_or_uint,_atomic_or_32)
ALIAS(atomic_or_ulong,_atomic_or_64)
ALIAS(atomic_or_ptr,_atomic_or_64)

ALIAS(atomic_or_32_nv,_atomic_or_32_nv)
ALIAS(atomic_or_64_nv,_atomic_or_64_nv)
ALIAS(atomic_or_uint_nv,_atomic_or_32_nv)
ALIAS(atomic_or_ulong_nv,_atomic_or_64_nv)
ALIAS(atomic_or_ptr_nv,_atomic_or_64_nv)

ALIAS(atomic_swap_32,_atomic_swap_32)
ALIAS(atomic_swap_64,_atomic_swap_64)
ALIAS(atomic_swap_uint,_atomic_swap_32)
ALIAS(atomic_swap_ulong,_atomic_swap_64)
ALIAS(atomic_swap_ptr,_atomic_swap_64)

ALIAS(atomic_cas_32,_atomic_cas_32)
ALIAS(atomic_cas_64,_atomic_cas_64)
ALIAS(atomic_cas_uint,_atomic_cas_32)
ALIAS(atomic_cas_ulong,_atomic_cas_64)
ALIAS(atomic_cas_ptr,_atomic_cas_64)

ALIAS(atomic_cas_32_ni,_atomic_cas_32_ni)
ALIAS(atomic_cas_64_ni,_atomic_cas_64_ni)
ALIAS(atomic_cas_uint_ni,_atomic_cas_32_ni)
ALIAS(atomic_cas_ulong_ni,_atomic_cas_64_ni)
ALIAS(atomic_cas_ptr_ni,_atomic_cas_64_ni)

ALIAS(membar_consumer,_membar_consumer)
ALIAS(membar_producer,_membar_producer)
ALIAS(membar_enter,_membar_consumer)
ALIAS(membar_exit,_membar_producer)
ALIAS(membar_sync,_membar_sync)

STRONG_ALIAS(_atomic_add_int,_atomic_add_32)
STRONG_ALIAS(_atomic_add_long,_atomic_add_64)
STRONG_ALIAS(_atomic_add_ptr,_atomic_add_64)

STRONG_ALIAS(_atomic_add_int_nv,_atomic_add_32_nv)
STRONG_ALIAS(_atomic_add_long_nv,_atomic_add_64_nv)
STRONG_ALIAS(_atomic_add_ptr_nv,_atomic_add_64_nv)

STRONG_ALIAS(_atomic_and_uint,_atomic_and_32)
STRONG_ALIAS(_atomic_and_ulong,_atomic_and_64)
STRONG_ALIAS(_atomic_and_ptr,_atomic_and_64)

STRONG_ALIAS(_atomic_and_uint_nv,_atomic_and_32_nv)
STRONG_ALIAS(_atomic_and_ulong_nv,_atomic_and_64_nv)
STRONG_ALIAS(_atomic_and_ptr_nv,_atomic_and_64_nv)

STRONG_ALIAS(_atomic_dec_uint,_atomic_dec_32)
STRONG_ALIAS(_atomic_dec_ulong,_atomic_dec_64)
STRONG_ALIAS(_atomic_dec_ptr,_atomic_dec_64)

STRONG_ALIAS(_atomic_dec_uint_nv,_atomic_dec_32_nv)
STRONG_ALIAS(_atomic_dec_ulong_nv,_atomic_dec_64_nv)
STRONG_ALIAS(_atomic_dec_ptr_nv,_atomic_dec_64_nv)

STRONG_ALIAS(_atomic_inc_uint,_atomic_inc_32)
STRONG_ALIAS(_atomic_inc_ulong,_atomic_inc_64)
STRONG_ALIAS(_atomic_inc_ptr,_atomic_inc_64)

STRONG_ALIAS(_atomic_inc_uint_nv,_atomic_inc_32_nv)
STRONG_ALIAS(_atomic_inc_ulong_nv,_atomic_inc_64_nv)
STRONG_ALIAS(_atomic_inc_ptr_nv,_atomic_inc_64_nv)

STRONG_ALIAS(_atomic_or_uint,_atomic_or_32)
STRONG_ALIAS(_atomic_or_ulong,_atomic_or_64)
STRONG_ALIAS(_atomic_or_ptr,_atomic_or_64)

STRONG_ALIAS(_atomic_or_uint_nv,_atomic_or_32_nv)
STRONG_ALIAS(_atomic_or_ulong_nv,_atomic_or_64_nv)
STRONG_ALIAS(_atomic_or_ptr_nv,_atomic_or_64_nv)

STRONG_ALIAS(_atomic_swap_uint,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ulong,_atomic_swap_64)
STRONG_ALIAS(_atomic_swap_ptr,_atomic_swap_64)

STRONG_ALIAS(_atomic_cas_uint,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong,_atomic_cas_64)
STRONG_ALIAS(_atomic_cas_ptr,_atomic_cas_64)

STRONG_ALIAS(_atomic_cas_uint_ni,_atomic_cas_32_ni)
STRONG_ALIAS(_atomic_cas_ulong_ni,_atomic_cas_64_ni)
STRONG_ALIAS(_atomic_cas_ptr_ni,_atomic_cas_64_ni)

STRONG_ALIAS(_membar_enter,_membar_consumer)
STRONG_ALIAS(_membar_exit,_membar_producer)
