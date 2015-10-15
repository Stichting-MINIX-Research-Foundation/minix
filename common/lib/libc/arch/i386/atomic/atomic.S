/*	$NetBSD: atomic.S,v 1.22 2014/05/23 03:17:31 uebayasi Exp $	*/

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
/*
 * __HAVE_ constants should not be in <machine/types.h>
 * because we can't use them from assembly. OTOH we
 * only need __HAVE_ATOMIC64_OPS here, and we don't.
 */
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

ENTRY(_atomic_add_32)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	LOCK(1)
	addl	%eax, (%edx)
	ret
END(_atomic_add_32)

ENTRY(_atomic_add_32_nv)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	%eax, %ecx
	LOCK(2)
	xaddl	%eax, (%edx)
	addl	%ecx, %eax
	ret
END(_atomic_add_32_nv)

ENTRY(_atomic_and_32)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	LOCK(3)
	andl	%eax, (%edx)
	ret
END(_atomic_and_32)

ENTRY(_atomic_and_32_nv)
	movl	4(%esp), %edx
	movl	(%edx), %eax
0:
	movl	%eax, %ecx
	andl	8(%esp), %ecx
	LOCK(4)
	cmpxchgl %ecx, (%edx)
	jnz	1f
	movl	%ecx, %eax
	ret
1:
	jmp	0b
END(_atomic_and_32_nv)

ENTRY(_atomic_dec_32)
	movl	4(%esp), %edx
	LOCK(5)
	decl	(%edx)
	ret
END(_atomic_dec_32)

ENTRY(_atomic_dec_32_nv)
	movl	4(%esp), %edx
	movl	$-1, %eax
	LOCK(6)
	xaddl	%eax, (%edx)
	decl	%eax
	ret
END(_atomic_dec_32_nv)

ENTRY(_atomic_inc_32)
	movl	4(%esp), %edx
	LOCK(7)
	incl	(%edx)
	ret
END(_atomic_inc_32)

ENTRY(_atomic_inc_32_nv)
	movl	4(%esp), %edx
	movl	$1, %eax
	LOCK(8)
	xaddl	%eax, (%edx)
	incl	%eax
	ret
END(_atomic_inc_32_nv)

ENTRY(_atomic_or_32)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	LOCK(9)
	orl	%eax, (%edx)
	ret
END(_atomic_or_32)

ENTRY(_atomic_or_32_nv)
	movl	4(%esp), %edx
	movl	(%edx), %eax
0:
	movl	%eax, %ecx
	orl	8(%esp), %ecx
	LOCK(10)
	cmpxchgl %ecx, (%edx)
	jnz	1f
	movl	%ecx, %eax
	ret
1:
	jmp	0b
END(_atomic_or_32_nv)

ENTRY(_atomic_swap_32)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	xchgl	%eax, (%edx)
	ret
END(_atomic_swap_32)

ENTRY(_atomic_cas_32)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	LOCK(12)
	cmpxchgl %ecx, (%edx)
	/* %eax now contains the old value */
	ret
END(_atomic_cas_32)

ENTRY(_atomic_cas_32_ni)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	cmpxchgl %ecx, (%edx)
	/* %eax now contains the old value */
	ret
END(_atomic_cas_32_ni)

ENTRY(_membar_consumer)
	LOCK(13)
	addl	$0, -4(%esp)
	ret
END(_membar_consumer)
ENDLABEL(membar_consumer_end)

ENTRY(_membar_producer)
	/* A store is enough */
	movl	$0, -4(%esp)
	ret
END(_membar_producer)
ENDLABEL(membar_producer_end)

ENTRY(_membar_sync)
	LOCK(14)
	addl	$0, -4(%esp)
	ret
END(_membar_sync)
ENDLABEL(membar_sync_end)

#if defined(__HAVE_ATOMIC64_OPS) || defined(_KERNEL)
ENTRY(_atomic_cas_64)
#ifdef _HARDKERNEL
	pushf
	cli
#endif /* _HARDKERNEL */
	pushl	%edi
	pushl	%ebx
	movl	12(%esp), %edi
	movl	16(%esp), %eax
	movl	20(%esp), %edx
	movl	24(%esp), %ebx
	movl	28(%esp), %ecx
	cmpl	0(%edi), %eax
	jne	2f
	cmpl	4(%edi), %edx
	jne	2f
	movl	%ebx, 0(%edi)
	movl	%ecx, 4(%edi)
1:
	popl	%ebx
	popl	%edi
#ifdef _HARDKERNEL
	popf
#endif /* _HARDKERNEL */
	ret
2:
	movl	0(%edi), %eax
	movl	4(%edi), %edx
	jmp	1b
END(_atomic_cas_64)
ENDLABEL(_atomic_cas_64_end)

ENTRY(_atomic_cas_cx8)
	pushl	%edi
	pushl	%ebx
	movl	12(%esp), %edi
	movl	16(%esp), %eax
	movl	20(%esp), %edx
	movl	24(%esp), %ebx
	movl	28(%esp), %ecx
	LOCK(15)
	cmpxchg8b (%edi)
	popl	%ebx
	popl	%edi
	ret
#ifdef _HARDKERNEL
#ifdef GPROF
	.space	16, 0x90
#else
	.space	32, 0x90
#endif
#endif /* _HARDKERNEL */
END(_atomic_cas_cx8)
ENDLABEL(_atomic_cas_cx8_end)
#endif /* __HAVE_ATOMIC64_OPS || _KERNEL */

#ifdef _HARDKERNEL
ENTRY(sse2_lfence)
	lfence
	ret
END(sse2_lfence)
ENDLABEL(sse2_lfence_end)

ENTRY(sse2_mfence)
	mfence
	ret
END(sse2_mfence)
ENDLABEL(sse2_mfence_end)

atomic_lockpatch:
	.globl	atomic_lockpatch
	.long	.Lpatch1, .Lpatch2, .Lpatch3, .Lpatch4, .Lpatch5
	.long	.Lpatch6, .Lpatch7, .Lpatch8, .Lpatch9, .Lpatch10
	.long	.Lpatch12, .Lpatch13, .Lpatch14, .Lpatch15, 0
#endif	/* _HARDKERNEL */

ALIAS(atomic_add_32,_atomic_add_32)
ALIAS(atomic_add_int,_atomic_add_32)
ALIAS(atomic_add_long,_atomic_add_32)
ALIAS(atomic_add_ptr,_atomic_add_32)

ALIAS(atomic_add_32_nv,_atomic_add_32_nv)
ALIAS(atomic_add_int_nv,_atomic_add_32_nv)
ALIAS(atomic_add_long_nv,_atomic_add_32_nv)
ALIAS(atomic_add_ptr_nv,_atomic_add_32_nv)

ALIAS(atomic_and_32,_atomic_and_32)
ALIAS(atomic_and_uint,_atomic_and_32)
ALIAS(atomic_and_ulong,_atomic_and_32)
ALIAS(atomic_and_ptr,_atomic_and_32)

ALIAS(atomic_and_32_nv,_atomic_and_32_nv)
ALIAS(atomic_and_uint_nv,_atomic_and_32_nv)
ALIAS(atomic_and_ulong_nv,_atomic_and_32_nv)
ALIAS(atomic_and_ptr_nv,_atomic_and_32_nv)

ALIAS(atomic_dec_32,_atomic_dec_32)
ALIAS(atomic_dec_uint,_atomic_dec_32)
ALIAS(atomic_dec_ulong,_atomic_dec_32)
ALIAS(atomic_dec_ptr,_atomic_dec_32)

ALIAS(atomic_dec_32_nv,_atomic_dec_32_nv)
ALIAS(atomic_dec_uint_nv,_atomic_dec_32_nv)
ALIAS(atomic_dec_ulong_nv,_atomic_dec_32_nv)
ALIAS(atomic_dec_ptr_nv,_atomic_dec_32_nv)

ALIAS(atomic_inc_32,_atomic_inc_32)
ALIAS(atomic_inc_uint,_atomic_inc_32)
ALIAS(atomic_inc_ulong,_atomic_inc_32)
ALIAS(atomic_inc_ptr,_atomic_inc_32)

ALIAS(atomic_inc_32_nv,_atomic_inc_32_nv)
ALIAS(atomic_inc_uint_nv,_atomic_inc_32_nv)
ALIAS(atomic_inc_ulong_nv,_atomic_inc_32_nv)
ALIAS(atomic_inc_ptr_nv,_atomic_inc_32_nv)

ALIAS(atomic_or_32,_atomic_or_32)
ALIAS(atomic_or_uint,_atomic_or_32)
ALIAS(atomic_or_ulong,_atomic_or_32)
ALIAS(atomic_or_ptr,_atomic_or_32)

ALIAS(atomic_or_32_nv,_atomic_or_32_nv)
ALIAS(atomic_or_uint_nv,_atomic_or_32_nv)
ALIAS(atomic_or_ulong_nv,_atomic_or_32_nv)
ALIAS(atomic_or_ptr_nv,_atomic_or_32_nv)

ALIAS(atomic_swap_32,_atomic_swap_32)
ALIAS(atomic_swap_uint,_atomic_swap_32)
ALIAS(atomic_swap_ulong,_atomic_swap_32)
ALIAS(atomic_swap_ptr,_atomic_swap_32)

ALIAS(atomic_cas_32,_atomic_cas_32)
ALIAS(atomic_cas_uint,_atomic_cas_32)
ALIAS(atomic_cas_ulong,_atomic_cas_32)
ALIAS(atomic_cas_ptr,_atomic_cas_32)

ALIAS(atomic_cas_32_ni,_atomic_cas_32_ni)
ALIAS(atomic_cas_uint_ni,_atomic_cas_32_ni)
ALIAS(atomic_cas_ulong_ni,_atomic_cas_32_ni)
ALIAS(atomic_cas_ptr_ni,_atomic_cas_32_ni)

#if defined(__HAVE_ATOMIC64_OPS) || defined(_KERNEL)
ALIAS(atomic_cas_64,_atomic_cas_64)
ALIAS(atomic_cas_64_ni,_atomic_cas_64)
ALIAS(__sync_val_compare_and_swap_8,_atomic_cas_64)
#endif /* __HAVE_ATOMIC64_OPS || _KERNEL */

ALIAS(membar_consumer,_membar_consumer)
ALIAS(membar_producer,_membar_producer)
ALIAS(membar_enter,_membar_consumer)
ALIAS(membar_exit,_membar_producer)
ALIAS(membar_sync,_membar_sync)

STRONG_ALIAS(_atomic_add_int,_atomic_add_32)
STRONG_ALIAS(_atomic_add_long,_atomic_add_32)
STRONG_ALIAS(_atomic_add_ptr,_atomic_add_32)

STRONG_ALIAS(_atomic_add_int_nv,_atomic_add_32_nv)
STRONG_ALIAS(_atomic_add_long_nv,_atomic_add_32_nv)
STRONG_ALIAS(_atomic_add_ptr_nv,_atomic_add_32_nv)

STRONG_ALIAS(_atomic_and_uint,_atomic_and_32)
STRONG_ALIAS(_atomic_and_ulong,_atomic_and_32)
STRONG_ALIAS(_atomic_and_ptr,_atomic_and_32)

STRONG_ALIAS(_atomic_and_uint_nv,_atomic_and_32_nv)
STRONG_ALIAS(_atomic_and_ulong_nv,_atomic_and_32_nv)
STRONG_ALIAS(_atomic_and_ptr_nv,_atomic_and_32_nv)

STRONG_ALIAS(_atomic_dec_uint,_atomic_dec_32)
STRONG_ALIAS(_atomic_dec_ulong,_atomic_dec_32)
STRONG_ALIAS(_atomic_dec_ptr,_atomic_dec_32)

STRONG_ALIAS(_atomic_dec_uint_nv,_atomic_dec_32_nv)
STRONG_ALIAS(_atomic_dec_ulong_nv,_atomic_dec_32_nv)
STRONG_ALIAS(_atomic_dec_ptr_nv,_atomic_dec_32_nv)

STRONG_ALIAS(_atomic_inc_uint,_atomic_inc_32)
STRONG_ALIAS(_atomic_inc_ulong,_atomic_inc_32)
STRONG_ALIAS(_atomic_inc_ptr,_atomic_inc_32)

STRONG_ALIAS(_atomic_inc_uint_nv,_atomic_inc_32_nv)
STRONG_ALIAS(_atomic_inc_ulong_nv,_atomic_inc_32_nv)
STRONG_ALIAS(_atomic_inc_ptr_nv,_atomic_inc_32_nv)

STRONG_ALIAS(_atomic_or_uint,_atomic_or_32)
STRONG_ALIAS(_atomic_or_ulong,_atomic_or_32)
STRONG_ALIAS(_atomic_or_ptr,_atomic_or_32)

STRONG_ALIAS(_atomic_or_uint_nv,_atomic_or_32_nv)
STRONG_ALIAS(_atomic_or_ulong_nv,_atomic_or_32_nv)
STRONG_ALIAS(_atomic_or_ptr_nv,_atomic_or_32_nv)

STRONG_ALIAS(_atomic_swap_uint,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ulong,_atomic_swap_32)
STRONG_ALIAS(_atomic_swap_ptr,_atomic_swap_32)

STRONG_ALIAS(_atomic_cas_uint,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ptr,_atomic_cas_32)

STRONG_ALIAS(_atomic_cas_uint_ni,_atomic_cas_32_ni)
STRONG_ALIAS(_atomic_cas_ulong_ni,_atomic_cas_32_ni)
STRONG_ALIAS(_atomic_cas_ptr_ni,_atomic_cas_32_ni)

STRONG_ALIAS(_membar_enter,_membar_consumer)
STRONG_ALIAS(_membar_exit,_membar_producer)
