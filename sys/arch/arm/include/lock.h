/*	$NetBSD: lock.h,v 1.25 2013/08/18 04:31:08 matt Exp $	*/

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
static __inline unsigned char
__swp(__cpu_simple_lock_t __val, volatile __cpu_simple_lock_t *__ptr)
{
#ifdef _ARM_ARCH_6
	uint32_t __rv, __tmp;
	if (sizeof(*__ptr) == 1) {
		__asm volatile(
			"1:\t"
			"ldrexb\t%[__rv], [%[__ptr]]"			"\n\t"
			"cmp\t%[__rv],%[__val]"				"\n\t"
#ifdef __thumb__
			"itt\tne"					"\n\t"
#endif
			"strexbne\t%[__tmp], %[__val], [%[__ptr]]"	"\n\t"
			"cmpne\t%[__tmp], #0"				"\n\t"
			"bne\t1b"					"\n\t"
#ifdef _ARM_ARCH_7
			"dmb"
#else
			"mcr\tp15, 0, %[__tmp], c7, c10, 5"
#endif
		    : [__rv] "=&r" (__rv), [__tmp] "=&r"(__tmp)
		    : [__val] "r" (__val), [__ptr] "r" (__ptr) : "cc", "memory");
	} else {
		__asm volatile(
			"1:\t"
			"ldrex\t%[__rv], [%[__ptr]]"			"\n\t"
			"cmp\t%[__rv],%[__val]"				"\n\t"
#ifdef __thumb__
			"itt\tne"					"\n\t"
#endif
			"strexne\t%[__tmp], %[__val], [%[__ptr]]"	"\n\t"
			"cmpne\t%[__tmp], #0"				"\n\t"
			"bne\t1b"					"\n\t"
#ifdef _ARM_ARCH_7
			"nop"
#else
			"mcr\tp15, 0, %[__tmp], c7, c10, 5"
#endif
		    : [__rv] "=&r" (__rv), [__tmp] "=&r"(__tmp)
		    : [__val] "r" (__val), [__ptr] "r" (__ptr) : "cc", "memory");
	}
	return __rv;
#else
	uint32_t __val32;
	__asm volatile("swpb %0, %1, [%2]"
	    : "=&r" (__val32) : "r" (__val), "r" (__ptr) : "memory");
	return __val32;
#endif
}
#else
/*
 * On Cortex-A9 (SMP), SWP no longer guarantees atomic results.  Thus we pad
 * out SWP so that when the A9 generates an undefined exception we can replace
 * the SWP/MOV instructions with the right LDREX/STREX instructions.
 *
 * This is why we force the SWP into the template needed for LDREX/STREX
 * including the extra instructions and extra register for testing the result.
 */
static __inline int
__swp(int __val, volatile int *__ptr)
{
	int __rv, __tmp;
	__asm volatile(
		"1:\t"
#ifdef _ARM_ARCH_6
		"ldrex\t%[__rv], [%[__ptr]]"			"\n\t"
		"cmp\t%[__rv],%[__val]"				"\n\t"
#ifdef __thumb__
		"it\tne"					"\n\t"
#endif
		"strexne\t%[__tmp], %[__val], [%[__ptr]]"	"\n\t"
#else
		"swp\t%[__rv], %[__val], [%[__ptr]]"		"\n\t"
		"mov\t%[__tmp], #0"				"\n\t"
		"cmp\t%[__rv],%[__val]"				"\n\t"
#endif
		"cmpne\t%[__tmp], #0"				"\n\t"
		"bne\t1b"					"\n\t"
#ifdef _ARM_ARCH_7
		"dmb"
#elif defined(_ARM_ARCH_6)
		"mcr\tp15, 0, %[__tmp], c7, c10, 5"
#else
		"nop"
#endif
	    : [__rv] "=&r" (__rv), [__tmp] "=&r"(__tmp)
	    : [__val] "r" (__val), [__ptr] "r" (__ptr) : "cc", "memory");
	return __rv;
}
#endif /* _KERNEL */

static __inline void __unused
__cpu_simple_lock_init(__cpu_simple_lock_t *alp)
{

	*alp = __SIMPLELOCK_UNLOCKED;
#ifdef _ARM_ARCH_7
	__asm __volatile("dsb");
#endif
}

#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
static __inline void __unused
__cpu_simple_lock(__cpu_simple_lock_t *alp)
{

	while (__swp(__SIMPLELOCK_LOCKED, alp) != __SIMPLELOCK_UNLOCKED)
		continue;
}
#else
void __cpu_simple_lock(__cpu_simple_lock_t *);
#endif

#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
static __inline int __unused
__cpu_simple_lock_try(__cpu_simple_lock_t *alp)
{

	return (__swp(__SIMPLELOCK_LOCKED, alp) == __SIMPLELOCK_UNLOCKED);
}
#else
int __cpu_simple_lock_try(__cpu_simple_lock_t *);
#endif

static __inline void __unused
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
