/*	$NetBSD: atomic_op_namespace.h,v 1.4 2008/06/23 10:33:52 ad Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
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

#ifndef _ATOMIC_OP_NAMESPACE_H_
#define	_ATOMIC_OP_NAMESPACE_H_

#include <sys/cdefs.h>

#if !defined(__lint__)

#define	atomic_add_32		_atomic_add_32
#define	atomic_add_int		_atomic_add_int
#define	atomic_add_long		_atomic_add_long
#define	atomic_add_ptr		_atomic_add_ptr
#define	atomic_add_64		_atomic_add_64

#define	atomic_add_32_nv	_atomic_add_32_nv
#define	atomic_add_int_nv	_atomic_add_int_nv
#define	atomic_add_long_nv	_atomic_add_long_nv
#define	atomic_add_ptr_nv	_atomic_add_ptr_nv
#define	atomic_add_64_nv	_atomic_add_64_nv

#define	atomic_and_32		_atomic_and_32
#define	atomic_and_uint		_atomic_and_uint
#define	atomic_and_ulong	_atomic_and_ulong
#define	atomic_and_64		_atomic_and_64

#define	atomic_and_32_nv	_atomic_and_32_nv
#define	atomic_and_uint_nv	_atomic_and_uint_nv
#define	atomic_and_ulong_nv	_atomic_and_ulong_nv
#define	atomic_and_64_nv	_atomic_and_64_nv

#define	atomic_or_32		_atomic_or_32
#define	atomic_or_uint		_atomic_or_uint
#define	atomic_or_ulong		_atomic_or_ulong
#define	atomic_or_64		_atomic_or_64

#define	atomic_or_32_nv		_atomic_or_32_nv
#define	atomic_or_uint_nv	_atomic_or_uint_nv
#define	atomic_or_ulong_nv	_atomic_or_ulong_nv
#define	atomic_or_64_nv		_atomic_or_64_nv

#define	atomic_cas_32		_atomic_cas_32
#define	atomic_cas_uint		_atomic_cas_uint
#define	atomic_cas_ulong	_atomic_cas_ulong
#define	atomic_cas_ptr		_atomic_cas_ptr
#define	atomic_cas_64		_atomic_cas_64

#define	atomic_cas_32_ni	_atomic_cas_32_ni
#define	atomic_cas_uint_ni	_atomic_cas_uint_ni
#define	atomic_cas_ulong_ni	_atomic_cas_ulong_ni
#define	atomic_cas_ptr_ni	_atomic_cas_ptr_ni
#define	atomic_cas_64_ni	_atomic_cas_64_ni

#define	atomic_swap_32		_atomic_swap_32
#define	atomic_swap_uint	_atomic_swap_uint
#define	atomic_swap_ulong	_atomic_swap_ulong
#define	atomic_swap_ptr		_atomic_swap_ptr
#define	atomic_swap_64		_atomic_swap_64

#define	atomic_dec_32		_atomic_dec_32
#define	atomic_dec_uint		_atomic_dec_uint
#define	atomic_dec_ulong	_atomic_dec_ulong
#define	atomic_dec_ptr		_atomic_dec_ptr
#define	atomic_dec_64		_atomic_dec_64

#define	atomic_dec_32_nv	_atomic_dec_32_nv
#define	atomic_dec_uint_nv	_atomic_dec_uint_nv
#define	atomic_dec_ptr_nv	_atomic_dec_ptr_nv
#define	atomic_dec_64_nv	_atomic_dec_64_nv

#define	atomic_inc_32		_atomic_inc_32
#define	atomic_inc_uint		_atomic_inc_uint
#define	atomic_inc_ulong	_atomic_inc_ulong
#define	atomic_inc_ptr		_atomic_inc_ptr
#define	atomic_inc_64		_atomic_inc_64

#define	atomic_inc_32_nv	_atomic_inc_32_nv
#define	atomic_inc_uint_nv	_atomic_inc_uint_nv
#define	atomic_inc_ptr_nv	_atomic_inc_ptr_nv
#define	atomic_inc_64_nv	_atomic_inc_64_nv

#define	membar_enter		_membar_enter
#define	membar_exit		_membar_exit
#define	membar_producer		_membar_producer
#define	membar_consumer		_membar_consumer
#define	membar_sync		_membar_sync

#endif /* __lint__ */

#if defined(_KERNEL)
#define	atomic_op_alias(a,s)	__strong_alias(a,s)
#else
#define	atomic_op_alias(a,s)	__weak_alias(a,s)
#endif /* _KERNEL */

#endif /* _ATOMIC_OP_NAMESPACE_H_ */
