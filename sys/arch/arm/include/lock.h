/*	$NetBSD: lock.h,v 1.21 2012/08/31 17:29:08 matt Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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

/*
 * Machine-dependent spin lock operations.
 *
 * NOTE: The SWP insn used here is available only on ARM architecture
 * version 3 and later (as well as 2a).  What we are going to do is
 * expect that the kernel will trap and emulate the insn.  That will
 * be slow, but give us the atomicity that we need.
 */

#ifndef _ARM_LOCK_H_
#define	_ARM_LOCK_H_

static __inline int
__SIMPLELOCK_LOCKED_P(__cpu_simple_lock_t *__ptr)
{
	return *__ptr == __SIMPLELOCK_LOCKED;
}

static __inline int
__SIMPLELOCK_UNLOCKED_P(__cpu_simple_lock_t *__ptr)
{
	return *__ptr == __SIMPLELOCK_UNLOCKED;
}

static __inline void
__cpu_simple_lock_clear(__cpu_simple_lock_t *__ptr)
{
	*__ptr = __SIMPLELOCK_UNLOCKED;
}

static __inline void
__cpu_simple_lock_set(__cpu_simple_lock_t *__ptr)
{
	*__ptr = __SIMPLELOCK_LOCKED;
}

#ifdef _KERNEL
#include <arm/cpufunc.h>

#define	mb_read		drain_writebuf		/* in cpufunc.h */
#define	mb_write	drain_writebuf		/* in cpufunc.h */
#define	mb_memory	drain_writebuf		/* in cpufunc.h */
#endif

#if defined(_KERNEL)
static __inline int
__swp(int __val, volatile unsigned char *__ptr)
{

	__asm volatile("swpb %0, %1, [%2]"
	    : "=&r" (__val) : "r" (__val), "r" (__ptr) : "memory");
	return __val;
}
#else
static __inline int
__swp(int __val, volatile int *__ptr)
{

	__asm volatile("swp %0, %1, [%2]"
	    : "=&r" (__val) : "r" (__val), "r" (__ptr) : "memory");
	return __val;
}
#endif /* _KERNEL */

static __inline void __attribute__((__unused__))
__cpu_simple_lock_init(__cpu_simple_lock_t *alp)
{

	*alp = __SIMPLELOCK_UNLOCKED;
#ifdef _ARM_ARCH_7
	__asm __volatile("dsb");
#endif
}

static __inline void __attribute__((__unused__))
__cpu_simple_lock(__cpu_simple_lock_t *alp)
{

	while (__swp(__SIMPLELOCK_LOCKED, alp) != __SIMPLELOCK_UNLOCKED)
		continue;
}

static __inline int __attribute__((__unused__))
__cpu_simple_lock_try(__cpu_simple_lock_t *alp)
{

	return (__swp(__SIMPLELOCK_LOCKED, alp) == __SIMPLELOCK_UNLOCKED);
}

static __inline void __attribute__((__unused__))
__cpu_simple_unlock(__cpu_simple_lock_t *alp)
{

#ifdef _ARM_ARCH_7
	__asm __volatile("dmb");
#endif
	*alp = __SIMPLELOCK_UNLOCKED;
#ifdef _ARM_ARCH_7
	__asm __volatile("dsb");
#endif
}

#endif /* _ARM_LOCK_H_ */
