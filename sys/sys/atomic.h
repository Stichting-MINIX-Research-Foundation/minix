/*	$NetBSD: atomic.h,v 1.11 2009/11/20 02:17:07 christos Exp $	*/

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

#ifndef _SYS_ATOMIC_H_
#define	_SYS_ATOMIC_H_

#include <sys/types.h>
#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <stdint.h>
#endif

__BEGIN_DECLS
/*
 * Atomic ADD
 */
void		atomic_add_32(volatile uint32_t *, int32_t);
void		atomic_add_int(volatile unsigned int *, int);
void		atomic_add_long(volatile unsigned long *, long);
void		atomic_add_ptr(volatile void *, ssize_t);
void		atomic_add_64(volatile uint64_t *, int64_t);

uint32_t	atomic_add_32_nv(volatile uint32_t *, int32_t);
unsigned int	atomic_add_int_nv(volatile unsigned int *, int);
unsigned long	atomic_add_long_nv(volatile unsigned long *, long);
void *		atomic_add_ptr_nv(volatile void *, ssize_t);
uint64_t	atomic_add_64_nv(volatile uint64_t *, int64_t);

/*
 * Atomic AND
 */
void		atomic_and_32(volatile uint32_t *, uint32_t);
void		atomic_and_uint(volatile unsigned int *, unsigned int);
void		atomic_and_ulong(volatile unsigned long *, unsigned long);
void		atomic_and_64(volatile uint64_t *, uint64_t);

uint32_t	atomic_and_32_nv(volatile uint32_t *, uint32_t);
unsigned int	atomic_and_uint_nv(volatile unsigned int *, unsigned int);
unsigned long	atomic_and_ulong_nv(volatile unsigned long *, unsigned long);
uint64_t	atomic_and_64_nv(volatile uint64_t *, uint64_t);

/*
 * Atomic OR
 */
void		atomic_or_32(volatile uint32_t *, uint32_t);
void		atomic_or_uint(volatile unsigned int *, unsigned int);
void		atomic_or_ulong(volatile unsigned long *, unsigned long);
void		atomic_or_64(volatile uint64_t *, uint64_t);

uint32_t	atomic_or_32_nv(volatile uint32_t *, uint32_t);
unsigned int	atomic_or_uint_nv(volatile unsigned int *, unsigned int);
unsigned long	atomic_or_ulong_nv(volatile unsigned long *, unsigned long);
uint64_t	atomic_or_64_nv(volatile uint64_t *, uint64_t);

/*
 * Atomic COMPARE-AND-SWAP
 */
uint32_t	atomic_cas_32(volatile uint32_t *, uint32_t, uint32_t);
unsigned int	atomic_cas_uint(volatile unsigned int *, unsigned int,
				unsigned int);
unsigned long	atomic_cas_ulong(volatile unsigned long *, unsigned long,
				 unsigned long);
void *		atomic_cas_ptr(volatile void *, void *, void *);
uint64_t	atomic_cas_64(volatile uint64_t *, uint64_t, uint64_t);

/*
 * Non-interlocked atomic COMPARE-AND-SWAP.
 */
uint32_t	atomic_cas_32_ni(volatile uint32_t *, uint32_t, uint32_t);
unsigned int	atomic_cas_uint_ni(volatile unsigned int *, unsigned int,
				   unsigned int);
unsigned long	atomic_cas_ulong_ni(volatile unsigned long *, unsigned long,
				    unsigned long);
void *		atomic_cas_ptr_ni(volatile void *, void *, void *);
uint64_t	atomic_cas_64_ni(volatile uint64_t *, uint64_t, uint64_t);

/*
 * Atomic SWAP
 */
uint32_t	atomic_swap_32(volatile uint32_t *, uint32_t);
unsigned int	atomic_swap_uint(volatile unsigned int *, unsigned int);
unsigned long	atomic_swap_ulong(volatile unsigned long *, unsigned long);
void *		atomic_swap_ptr(volatile void *, void *);
uint64_t	atomic_swap_64(volatile uint64_t *, uint64_t);

/*
 * Atomic DECREMENT
 */
void		atomic_dec_32(volatile uint32_t *);
void		atomic_dec_uint(volatile unsigned int *);
void		atomic_dec_ulong(volatile unsigned long *);
void		atomic_dec_ptr(volatile void *);
void		atomic_dec_64(volatile uint64_t *);

uint32_t	atomic_dec_32_nv(volatile uint32_t *);
unsigned int	atomic_dec_uint_nv(volatile unsigned int *);
unsigned long	atomic_dec_ulong_nv(volatile unsigned long *);
void *		atomic_dec_ptr_nv(volatile void *);
uint64_t	atomic_dec_64_nv(volatile uint64_t *);

/*
 * Atomic INCREMENT
 */
void		atomic_inc_32(volatile uint32_t *);
void		atomic_inc_uint(volatile unsigned int *);
void		atomic_inc_ulong(volatile unsigned long *);
void		atomic_inc_ptr(volatile void *);
void		atomic_inc_64(volatile uint64_t *);

uint32_t	atomic_inc_32_nv(volatile uint32_t *);
unsigned int	atomic_inc_uint_nv(volatile unsigned int *);
unsigned long	atomic_inc_ulong_nv(volatile unsigned long *);
void *		atomic_inc_ptr_nv(volatile void *);
uint64_t	atomic_inc_64_nv(volatile uint64_t *);

/*
 * Memory barrier operations
 */
void		membar_enter(void);
void		membar_exit(void);
void		membar_producer(void);
void		membar_consumer(void);
void		membar_sync(void);

__END_DECLS

#endif /* ! _SYS_ATOMIC_H_ */
