/*	$NetBSD: atomic.S,v 1.5 2009/11/09 14:22:02 skrll Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takayoshi Kochi.
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

#ifdef _KERNEL
#define	ALIAS(f, t)	STRONG_ALIAS(f,t)
#else
#define	ALIAS(f, t)	WEAK_ALIAS(f,t)
#endif

	.text

ENTRY(_atomic_dec_32,1)
	fetchadd4.rel	r8=[r32],-1
	br.ret.sptk	rp
END(_atomic_dec_32)

ENTRY(_atomic_dec_64,1)
	fetchadd8.rel	r8=[r32],-1
	br.ret.sptk	rp
END(_atomic_dec_64)

ENTRY(_atomic_dec_32_nv,1)
	fetchadd4.rel	r8=[r32],-1
	br.ret.sptk	rp
END(_atomic_dec_32_nv)

ENTRY(_atomic_dec_64_nv,1)
	fetchadd8.rel	r8=[r32],-1
	br.ret.sptk	rp
END(_atomic_dec_64_nv)

ENTRY(_atomic_inc_32,1)
	fetchadd4.rel	r8=[r32],1
	br.ret.sptk	rp
END(_atomic_inc_32)

ENTRY(_atomic_inc_64,1)
	fetchadd8.rel	r8=[r32],1
	br.ret.sptk	rp
END(_atomic_inc_64)

ENTRY(_atomic_inc_32_nv,1)
	fetchadd4.rel	r8=[r32],1
	br.ret.sptk	rp
END(_atomic_inc_32_nv)

ENTRY(_atomic_inc_64_nv,1)
	fetchadd8.rel	r8=[r32],1
	br.ret.sptk	rp
END(_atomic_inc_64_nv)

ENTRY(_atomic_swap_32,2)
	xchg4		r8=[r32],r33
	;;
	mov		r33=r8
	br.ret.sptk	rp
END(_atomic_swap_32)

ENTRY(_atomic_swap_64,2)
	xchg8		r8=[r32],r33
	;;
	mov		r33=r8
	br.ret.sptk	rp
END(_atomic_swap_64)

ENTRY(_atomic_cas_32,3)
	mov		ar.ccv=r33
	;;
	cmpxchg4.acq	r8=[r32],r34,ar.ccv
	br.ret.sptk	rp
END(_atomic_cas_32)

ENTRY(_atomic_cas_64,3)
	mov		ar.ccv=r33
	;;
	cmpxchg8.acq	r8=[r32],r34,ar.ccv
	br.ret.sptk	rp
END(_atomic_cas_64)

ENTRY(_membar_consumer,0)
	mf
	br.ret.sptk	rp
END(_membar_consumer)

ENTRY(_membar_producer,0)
	mf
	br.ret.sptk	rp
END(_membar_producer)

ENTRY(_membar_enter,0)
	mf
	br.ret.sptk	rp
END(_membar_enter)

ENTRY(_membar_exit,0)
	mf
	br.ret.sptk	rp
END(_membar_exit)

ENTRY(_membar_sync,0)
	mf
	br.ret.sptk	rp
END(_membar_sync)


ALIAS(atomic_add_32,_atomic_add_32)
ALIAS(atomic_add_int,_atomic_add_32)
ALIAS(atomic_add_64,_atomic_add_64)
ALIAS(atomic_add_long,_atomic_add_64)
ALIAS(atomic_add_ptr,_atomic_add_64)

ALIAS(atomic_add_32_nv,_atomic_add_32_nv)
ALIAS(atomic_add_int_nv,_atomic_add_32_nv)
ALIAS(atomic_add_64_nv,_atomic_add_64_nv)
ALIAS(atomic_add_long_nv,_atomic_add_64_nv)
ALIAS(atomic_add_ptr_nv,_atomic_add_64_nv)

ALIAS(atomic_and_32,_atomic_and_32)
ALIAS(atomic_and_uint,_atomic_and_32)
ALIAS(atomic_and_64,_atomic_and_64)
ALIAS(atomic_and_ulong,_atomic_and_64)
ALIAS(atomic_and_ptr,_atomic_and_64)

ALIAS(atomic_and_32_nv,_atomic_and_32_nv)
ALIAS(atomic_and_uint_nv,_atomic_and_32_nv)
ALIAS(atomic_and_64_nv,_atomic_and_64_nv)
ALIAS(atomic_and_ulong_nv,_atomic_and_64_nv)
ALIAS(atomic_and_ptr_nv,_atomic_and_64_nv)

ALIAS(atomic_dec_32,_atomic_dec_32)
ALIAS(atomic_dec_uint,_atomic_dec_32)
ALIAS(atomic_dec_64,_atomic_dec_64)
ALIAS(atomic_dec_ulong,_atomic_dec_64)
ALIAS(atomic_dec_ptr,_atomic_dec_64)

ALIAS(atomic_dec_32_nv,_atomic_dec_32_nv)
ALIAS(atomic_dec_uint_nv,_atomic_dec_32_nv)
ALIAS(atomic_dec_64_nv,_atomic_dec_64_nv)
ALIAS(atomic_dec_ulong_nv,_atomic_dec_64_nv)
ALIAS(atomic_dec_ptr_nv,_atomic_dec_64_nv)

ALIAS(atomic_inc_32,_atomic_inc_32)
ALIAS(atomic_inc_uint,_atomic_inc_32)
ALIAS(atomic_inc_64,_atomic_inc_64)
ALIAS(atomic_inc_ulong,_atomic_inc_64)
ALIAS(atomic_inc_ptr,_atomic_inc_64)

ALIAS(atomic_inc_32_nv,_atomic_inc_32_nv)
ALIAS(atomic_inc_uint_nv,_atomic_inc_32_nv)
ALIAS(atomic_inc_64_nv,_atomic_inc_64_nv)
ALIAS(atomic_inc_ulong_nv,_atomic_inc_64_nv)
ALIAS(atomic_inc_ptr_nv,_atomic_inc_64_nv)

ALIAS(atomic_or_32,_atomic_or_32)
ALIAS(atomic_or_uint,_atomic_or_32)
ALIAS(atomic_or_64,_atomic_or_64)
ALIAS(atomic_or_ulong,_atomic_or_64)
ALIAS(atomic_or_ptr,_atomic_or_64)

ALIAS(atomic_or_32_nv,_atomic_or_32_nv)
ALIAS(atomic_or_uint_nv,_atomic_or_32_nv)
ALIAS(atomic_or_64_nv,_atomic_or_64_nv)
ALIAS(atomic_or_ulong_nv,_atomic_or_64_nv)
ALIAS(atomic_or_ptr_nv,_atomic_or_64_nv)

ALIAS(atomic_swap_32,_atomic_swap_32)
ALIAS(atomic_swap_uint,_atomic_swap_32)
ALIAS(atomic_swap_64,_atomic_swap_64)
ALIAS(atomic_swap_ulong,_atomic_swap_64)
ALIAS(atomic_swap_ptr,_atomic_swap_64)

ALIAS(atomic_cas_32,_atomic_cas_32)
ALIAS(atomic_cas_uint,_atomic_cas_32)
ALIAS(atomic_cas_64,_atomic_cas_64)
ALIAS(atomic_cas_ulong,_atomic_cas_64)
ALIAS(atomic_cas_ptr,_atomic_cas_64)

ALIAS(atomic_cas_32_ni,_atomic_cas_32)
ALIAS(atomic_cas_uint_ni,_atomic_cas_32)
ALIAS(atomic_cas_64_ni,_atomic_cas_64)
ALIAS(atomic_cas_ulong_ni,_atomic_cas_64)
ALIAS(atomic_cas_ptr_ni,_atomic_cas_64)

ALIAS(membar_consumer,_membar_consumer)
ALIAS(membar_producer,_membar_producer)
ALIAS(membar_enter,_membar_enter)
ALIAS(membar_exit,_membar_exit)
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

STRONG_ALIAS(_atomic_cas_uint_ni,_atomic_cas_32)
STRONG_ALIAS(_atomic_cas_ulong_ni,_atomic_cas_64)
STRONG_ALIAS(_atomic_cas_ptr_ni,_atomic_cas_64)
